// SPDX-License-Identifier: GPL-3.0-only
// MONO2K's reference renderer. It boots an owner's Monomachine OS image in gearmulator's emulation and records what
// the machines do, so MONO2K's own engines can be tuned against the sound by ear and by measurement. The image is read
// from the path given and nowhere else: never copied, never shipped, and nothing here reads or translates its code.
// It is driven only as the instrument is: MIDI (the public MIDI spec's machine assignment and CC map) and the panel.
//
//   mm_ref ROM lcd DIR             the screen after assigning each machine number to track 1 (DIR/lcd_NN.pbm)
//   mm_ref ROM render DIR < SPEC   one WAV per line of SPEC: name machine note velocity gate_s tail_s [cc=value ...]
//   mm_ref ROM pages DIR           GND-SIN's init pages, as the screen shows them, one per press of the page key
#include "mdLib/mdhardware.h"
#include "mdLib/mdfrontpanel.h"
#include "mdLib/mdpanel.h"
#include "mdLib/mdromloader.h"
#include "baseLib/filesystem.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	constexpr uint32_t g_rate = md::g_samplerate;

	void require(bool _ok, const char* _what) { if(!_ok) throw std::runtime_error(_what); }

	void advance(md::Hardware& _hw, uint32_t _frames)
	{
		while(_frames) { const auto n = std::min<uint32_t>(256, _frames); _hw.advance(n); _frames -= n; }
	}

	void midi(md::Hardware& _hw, uint8_t _a, uint8_t _b, uint8_t _c)
	{
		require(_hw.sendMidi(synthLib::SMidiEvent(synthLib::MidiEventSource::Host, _a, _b, _c)), "MIDI rejected");
	}

	void tap(md::Hardware& _hw, md::PanelControl _control)
	{
		const auto p = md::panelPacket(md::MachineModel::Monomachine, _control);
		require(p.has_value(), "no such panel control");
		require(_hw.trySendPanelEvent(p->row, p->mask), "panel press rejected");
		advance(_hw, 2048);
		require(_hw.trySendPanelEvent(p->row, 0), "panel release rejected");
		advance(_hw, 4096);
	}

	// KIT > LOAD, FUNCTION + PLAY clears the kit, ENTER loads it: every track starts from the same empty kit
	void emptyKit(md::Hardware& _hw)
	{
		tap(_hw, md::PanelControl::Kit);
		tap(_hw, md::PanelControl::Enter);
		const auto fn = md::panelPacket(md::MachineModel::Monomachine, md::PanelControl::Function);
		const auto play = md::panelPacket(md::MachineModel::Monomachine, md::PanelControl::Play);
		require(fn && play, "no clear-kit controls");
		md::PanelRowState rows;
		for(const auto p : {rows.press(*fn), rows.press(*play), rows.release(*play), rows.release(*fn)})
		{
			require(_hw.trySendPanelEvent(p.row, p.mask), "clear-kit control rejected");
			advance(_hw, 2048);
		}
		advance(_hw, g_rate * 2);
		tap(_hw, md::PanelControl::Enter);
		tap(_hw, md::PanelControl::Exit);
	}

	// the MIDI spec's machine assignment: track, machine number, init 1 sets every data page to its default
	void assign(md::Hardware& _hw, uint8_t _track, uint8_t _machine)
	{
		synthLib::SMidiEvent e(synthLib::MidiEventSource::Host);
		e.sysex = {0xf0, 0, 0x20, 0x3c, 3, 0, 0x5b, _track, _machine, 1, 0xf7};
		require(_hw.sendMidi(e), "machine assignment rejected");
		advance(_hw, g_rate);
	}

	std::unique_ptr<md::Hardware> boot(const char* _rom)
	{
		std::vector<uint8_t> rom;
		require(baseLib::filesystem::readFile(rom, _rom), "could not read the OS image");
		require(md::RomLoader::isRomForModel(rom, md::MachineModel::Monomachine), "not a Monomachine OS image");
		auto hw = std::make_unique<md::Hardware>(rom, _rom, md::MachineModel::Monomachine);
		advance(*hw, g_rate * 20);
		require(hw->isAudioReady() && hw->isFirmwareMidiReady(), "the OS did not finish booting");
		emptyKit(*hw);
		return hw;
	}

	void writePbm(const std::string& _path, const md::FrontPanel& _panel)
	{
		FILE* f = std::fopen(_path.c_str(), "wb");
		require(f, "could not write the screen");
		const auto w = md::FrontPanel::g_lcdWidth, h = md::FrontPanel::g_lcdHeight;
		std::fprintf(f, "P1\n%u %u\n", w, h);
		for(uint32_t y = 0; y < h; ++y)
		{
			for(uint32_t x = 0; x < w; ++x) std::fputs(_panel.getLcdPixel(x, y) ? "1 " : "0 ", f);
			std::fputc('\n', f);
		}
		std::fclose(f);
	}

	void writeWav(const std::string& _path, const std::vector<float>& _lr)
	{
		FILE* f = std::fopen(_path.c_str(), "wb");
		require(f, "could not write the WAV");
		// 32-bit float, unclipped: quiet measurements keep the DSP's own resolution instead of a 16-bit floor
		const uint32_t frames = static_cast<uint32_t>(_lr.size() / 2), bytes = frames * 8;
		auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
		auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
		std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
		u32(16); u16(3); u16(2); u32(g_rate); u32(g_rate * 8); u16(8); u16(32);
		std::fwrite("data", 1, 4, f); u32(bytes);
		std::fwrite(_lr.data(), sizeof(float), _lr.size(), f);
		std::fclose(f);
	}

	void render(md::Hardware& _hw, uint32_t _frames, std::vector<float>& _out)
	{
		std::array<std::array<float, 256>, 2> buf{};
		synthLib::TAudioOutputs outs{};
		outs[0] = buf[0].data();
		outs[1] = buf[1].data();
		while(_frames)
		{
			const auto n = std::min<uint32_t>(256, _frames);
			_hw.processAudio(outs, n, 0);
			for(uint32_t i = 0; i < n; ++i) { _out.push_back(buf[0][i]); _out.push_back(buf[1][i]); }
			_frames -= n;
		}
	}

	int lcd(const char* _rom, const std::string& _dir)
	{
		auto hw = boot(_rom);
		for(uint8_t m = 0; m < 48; ++m)
		{
			assign(*hw, 0, m);
			advance(*hw, g_rate / 2);
			char name[32];
			std::snprintf(name, sizeof name, "/lcd_%02u.pbm", m);
			writePbm(_dir + name, hw->getFrontPanelSnapshot());
		}
		return 0;
	}

	int pages(const char* _rom, const std::string& _dir)
	{
		auto hw = boot(_rom);
		assign(*hw, 0, 1);
		for(unsigned i = 0; i < 8; ++i)
		{
			char name[32];
			std::snprintf(name, sizeof name, "/page_%u.pbm", i);
			writePbm(_dir + name, hw->getFrontPanelSnapshot());
			tap(*hw, md::PanelControl::DataPageForward);
			advance(*hw, g_rate * 3 / 2);           // the controllers land one by one on the OS: let them all arrive
		}
		return 0;
	}

	int renderSpec(const char* _rom, const std::string& _dir)
	{
		auto hw = boot(_rom);
		std::string line;
		while(std::getline(std::cin, line))
		{
			if(line.empty() || line[0] == '#') continue;
			std::istringstream in(line);
			std::string name;
			unsigned machine, note, vel;
			double gate, tail;
			require(static_cast<bool>(in >> name >> machine >> note >> vel >> gate >> tail), "bad spec line");
			assign(*hw, 0, static_cast<uint8_t>(machine));
			std::string kv;
			while(in >> kv)
			{
				const auto eq = kv.find('=');
				require(eq != std::string::npos, "cc=value expected");
				midi(*hw, 0xb0, static_cast<uint8_t>(std::stoi(kv.substr(0, eq))), static_cast<uint8_t>(std::stoi(kv.substr(eq + 1))));
			}
			advance(*hw, g_rate * 3 / 2);           // the controllers land one by one on the OS: let them all arrive
			std::vector<float> lr;
			midi(*hw, 0x90, static_cast<uint8_t>(note), static_cast<uint8_t>(vel));
			render(*hw, static_cast<uint32_t>(gate * g_rate), lr);
			midi(*hw, 0x80, static_cast<uint8_t>(note), 0);
			render(*hw, static_cast<uint32_t>(tail * g_rate), lr);
			writeWav(_dir + "/" + name + ".wav", lr);
			std::cout << name << '\n';
			advance(*hw, g_rate / 2);
		}
		return 0;
	}
}

int main(int _argc, char** _argv)
{
	if(_argc != 4) { std::cerr << "mm_ref ROM lcd|render DIR\n"; return 2; }
	try
	{
		const std::string cmd = _argv[2];
		if(cmd == "lcd") return lcd(_argv[1], _argv[3]);
		if(cmd == "render") return renderSpec(_argv[1], _argv[3]);
		if(cmd == "pages") return pages(_argv[1], _argv[3]);
		std::cerr << "unknown command\n";
		return 2;
	}
	catch(const std::exception& e)
	{
		std::cerr << "mm_ref: " << e.what() << '\n';
		return 1;
	}
}
