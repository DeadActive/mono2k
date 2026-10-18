// SPDX-License-Identifier: GPL-3.0-only
// MONO2K's reference renderer. It boots an owner's Monomachine OS image in gearmulator's emulation and records what
// the machines do, so MONO2K's own engines can be tuned against the sound by ear and by measurement. The image is read
// from the path given and nowhere else: never copied, never shipped, and nothing here reads or translates its code.
// It is driven only as the instrument is: MIDI (the public MIDI spec's machine assignment and CC map) and the panel.
//
//   mm_ref ROM lcd DIR             the screen after assigning each machine number to track 1 (DIR/lcd_NN.pbm)
//   mm_ref ROM render DIR < SPEC   one WAV per line of SPEC: name machine note velocity gate_s tail_s [cc=value ...]
//                                  (mm:cc=value for this renderer alone, m2k:cc=value skipped)
//   mm_ref ROM pages DIR           GND-SIN's init pages, as the screen shows them, one per press of the page key
//   mm_ref ROM loop DIR < LOOP     a loop over several tracks (tools/ref/power.loop), DIR/loop_mm.wav
//   mm_ref ROM values DIR M CC     machine M's page as the screen shows each value 0..127 of controller CC
//   mm_ref ROM panel DIR < SCRIPT  panel presses, knob turns, screenshots, audio (tap / enc / assign / midi / wait / shot / record)
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
#include <optional>
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

	// two controls held together, released in the other order
	void chord(md::Hardware& _hw, md::PanelControl _a, md::PanelControl _b)
	{
		const auto a = md::panelPacket(md::MachineModel::Monomachine, _a);
		const auto b = md::panelPacket(md::MachineModel::Monomachine, _b);
		require(a && b, "no such panel control");
		md::PanelRowState rows;
		for(const auto p : {rows.press(*a), rows.press(*b), rows.release(*b), rows.release(*a)})
		{
			require(_hw.trySendPanelEvent(p.row, p.mask), "panel control rejected");
			advance(_hw, 2048);
		}
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

	// an FX machine assigned by MIDI hears nothing until its track's KIT > EDIT KIT TRACK page has been opened and
	// closed once (found by measurement: before it the FX's output is digital silence, after it REVERB and RINGMOD
	// carry the track before; the input chosen there, NEIBOR, is the default either way)
	void fxInput(md::Hardware& _hw, uint8_t _track)
	{
		tap(_hw, static_cast<md::PanelControl>(static_cast<uint8_t>(md::PanelControl::Track1) + _track));
		tap(_hw, md::PanelControl::Kit);
		tap(_hw, md::PanelControl::Down);
		tap(_hw, md::PanelControl::Enter);
		advance(_hw, g_rate * 3 / 10);
		tap(_hw, md::PanelControl::Exit);
		tap(_hw, md::PanelControl::Exit);
		advance(_hw, g_rate * 3 / 10);
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

	// a machine's page as its screen shows each value of one controller: what the instrument tells a player the value
	// means (a ratio, a waveform's name). DIR/val_MM_CC_VVV.pbm
	int values(const char* _rom, const std::string& _dir, uint8_t _machine, uint8_t _cc)
	{
		auto hw = boot(_rom);
		assign(*hw, 0, _machine);
		advance(*hw, g_rate / 2);
		for(unsigned v = 0; v < 128; ++v)
		{
			midi(*hw, 0xb0, _cc, static_cast<uint8_t>(v));
			advance(*hw, g_rate / 8);
			char name[48];
			std::snprintf(name, sizeof name, "/val_%02u_%02u_%03u.pbm", _machine, _cc, v);
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

	// a panel script from stdin, to find where a setting lives: "tap NAME" (a panel control, as gearmulator names it),
	// "enc N DELTA" (data entry knob N 0..7 turned), "assign TRACK MACHINE", "midi A B C", "wait MS", "shot NAME"
	// (DIR/NAME.pbm), "record SECONDS NAME" (DIR/NAME.wav)
	int panel(const char* _rom, const std::string& _dir)
	{
		auto hw = boot(_rom);
		std::string line;
		while(std::getline(std::cin, line))
		{
			if(line.empty() || line[0] == '#') continue;
			std::istringstream in(line);
			std::string cmd;
			in >> cmd;
			if(cmd == "tap")
			{
				std::string name;
				in >> name;
				bool found = false;
				for(uint8_t c = 0; c <= static_cast<uint8_t>(md::PanelControl::ClassicExtended); ++c)
					if(name == md::panelControlName(static_cast<md::PanelControl>(c))) { tap(*hw, static_cast<md::PanelControl>(c)); found = true; break; }
				require(found, "no such panel control");
			}
			else if(cmd == "enc")
			{
				int n = 0, d = 0;
				in >> n >> d;
				const auto c = md::panelEncoderCommand(md::MachineModel::Monomachine, static_cast<md::PanelEncoder>(n));
				require(c.has_value(), "no such knob");
				for(int k = 0; k < (d < 0 ? -d : d); ++k)
				{
					require(hw->trySendPanelEvent(*c, static_cast<uint8_t>(d < 0 ? 0xff : 0x01)), "knob turn rejected");
					advance(*hw, 1024);
				}
				advance(*hw, 4096);
			}
			else if(cmd == "chord")    // two controls held together: "chord Function Play"
			{
				std::string a, b;
				in >> a >> b;
				auto find = [](const std::string& n) {
					for(uint8_t c = 0; c <= static_cast<uint8_t>(md::PanelControl::ClassicExtended); ++c)
						if(n == md::panelControlName(static_cast<md::PanelControl>(c))) return std::optional<md::PanelControl>(static_cast<md::PanelControl>(c));
					return std::optional<md::PanelControl>();
				};
				const auto ca = find(a), cb = find(b);
				require(ca && cb, "no such panel control");
				chord(*hw, *ca, *cb);
			}
			else if(cmd == "assign") { unsigned t = 0, m = 0; in >> t >> m; assign(*hw, static_cast<uint8_t>(t), static_cast<uint8_t>(m)); }
			else if(cmd == "midi") { unsigned a = 0, b = 0, c = 0; in >> a >> b >> c; midi(*hw, static_cast<uint8_t>(a), static_cast<uint8_t>(b), static_cast<uint8_t>(c)); }
			else if(cmd == "wait") { unsigned ms = 0; in >> ms; advance(*hw, g_rate / 1000 * ms); }
			else if(cmd == "shot") { std::string name; in >> name; writePbm(_dir + "/" + name + ".pbm", hw->getFrontPanelSnapshot()); }
			else if(cmd == "record") { double sec = 0; std::string name; in >> sec >> name; std::vector<float> lr; render(*hw, static_cast<uint32_t>(sec * g_rate), lr); writeWav(_dir + "/" + name + ".wav", lr); }
			else require(false, "unknown panel command");
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
			// every line starts fresh: the OS keeps a machine running between notes (its oscillator feeding the filter), so
			// a line would inherit the last one's resonance (chain_order11). Another machine assigned does not reset it;
			// a note played on it does (chain_order11 measured both)
			assign(*hw, 0, static_cast<uint8_t>(machine == 2 ? 1 : 2));
			midi(*hw, 0x90, static_cast<uint8_t>(note), 100);
			advance(*hw, g_rate / 20);
			midi(*hw, 0x80, static_cast<uint8_t>(note), 0);
			advance(*hw, g_rate / 2);
			assign(*hw, 0, static_cast<uint8_t>(machine));
			std::string kv;
			while(in >> kv)
			{
				// "m2k:cc=v" is MONO2K's alone (a control it keeps elsewhere); "mm:cc=v" is this instrument's alone
				if(kv.rfind("m2k:", 0) == 0) continue;
				if(kv.rfind("mm:", 0) == 0) kv = kv.substr(3);
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

namespace
{
	// a loop over several tracks (tools/ref/power.loop): each track its machine and controls on its own MIDI
	// channel, then the notes played step by step at the tempo, from a fresh boot; DIR/loop_mm.wav
	int loop(const char* _rom, const std::string& _dir)
	{
		auto hw = boot(_rom);
		double tempo = 120, seconds = 15;
		unsigned steps = 16;
		struct Note { unsigned track, step, note, vel, gate; };
		std::vector<Note> notes;
		std::vector<std::pair<unsigned, unsigned>> seq;
		std::string line;
		while(std::getline(std::cin, line))
		{
			if(line.empty() || line[0] == '#') continue;
			std::istringstream in(line);
			std::string kind;
			in >> kind;
			if(kind == "L") require(static_cast<bool>(in >> tempo >> steps >> seconds), "bad L line");
			else if(kind == "N")
			{
				Note n{};
				require(static_cast<bool>(in >> n.track >> n.step >> n.note >> n.vel >> n.gate), "bad N line");
				notes.push_back(n);
			}
			else if(kind == "Q")       // a sequencer trig (an FX track opens on the OS's own trigs, not on MIDI notes)
			{
				unsigned track = 0, step = 0;
				require(static_cast<bool>(in >> track >> step), "bad Q line");
				seq.emplace_back(track, step);
			}
			else if(kind == "T")
			{
				unsigned track, machine;
				require(static_cast<bool>(in >> track >> machine), "bad T line");
				assign(*hw, static_cast<uint8_t>(track), static_cast<uint8_t>(machine));
				if(machine >= 12 && machine <= 17 && machine != 14) fxInput(*hw, static_cast<uint8_t>(track));
				std::string kv;
				while(in >> kv)
				{
					if(kv.rfind("m2k:", 0) == 0) continue;
					if(kv.rfind("mm:", 0) == 0) kv = kv.substr(3);
					const auto eq = kv.find('=');
					require(eq != std::string::npos, "cc=value expected");
					midi(*hw, static_cast<uint8_t>(0xb0 | track), static_cast<uint8_t>(std::stoi(kv.substr(0, eq))),
					     static_cast<uint8_t>(std::stoi(kv.substr(eq + 1))));
				}
			}
		}
		advance(*hw, g_rate * 2);               // the controllers land one by one on the OS: let them all arrive
		if(!seq.empty())                        // grid recording: the pattern's tracks cleared (the image's A01 holds a
		{                                       // demo), each Q trig placed, then the sequencer runs
			tap(*hw, md::PanelControl::Record);
			for(uint8_t t = 0; t < 6; ++t)
			{
				tap(*hw, static_cast<md::PanelControl>(static_cast<uint8_t>(md::PanelControl::Track1) + t));
				chord(*hw, md::PanelControl::Function, md::PanelControl::Play);    // CLEAR TRACK
			}
			for(const auto& q : seq)
			{
				tap(*hw, static_cast<md::PanelControl>(static_cast<uint8_t>(md::PanelControl::Track1) + q.first));
				tap(*hw, static_cast<md::PanelControl>(static_cast<uint8_t>(md::PanelControl::Trigger1) + q.second));
			}
			tap(*hw, md::PanelControl::Record);
			advance(*hw, g_rate / 2);
			tap(*hw, md::PanelControl::Play);
		}
		const double step_frames = g_rate * 15.0 / tempo;
		const uint32_t total = static_cast<uint32_t>(seconds * g_rate);
		std::vector<float> lr;
		std::vector<std::pair<uint32_t, uint32_t>> offs;     // (frame, channel << 8 | note)
		for(uint32_t s = 0, at = 0; at < total; ++s)
		{
			const uint32_t frame = static_cast<uint32_t>(s * step_frames + 0.5);
			const uint32_t next = static_cast<uint32_t>((s + 1) * step_frames + 0.5);
			for(auto it = offs.begin(); it != offs.end();)
				if(it->first <= frame) { midi(*hw, static_cast<uint8_t>(0x80 | (it->second >> 8)), static_cast<uint8_t>(it->second & 0xff), 0); it = offs.erase(it); }
				else ++it;
			for(const auto& n : notes)
				if(n.step == s % steps)
				{
					midi(*hw, static_cast<uint8_t>(0x90 | n.track), static_cast<uint8_t>(n.note), static_cast<uint8_t>(n.vel));
					offs.emplace_back(static_cast<uint32_t>((s + n.gate) * step_frames + 0.5) - 1, n.track << 8 | n.note);
				}
			const uint32_t n = std::min(next, total) - at;
			render(*hw, n, lr);
			at += n;
		}
		writeWav(_dir + "/loop_mm.wav", lr);
		std::cout << "loop_mm\n";
		return 0;
	}
}

int main(int _argc, char** _argv)
{
	if(_argc != 4 && _argc != 6) { std::cerr << "mm_ref ROM lcd|render|pages|loop DIR | mm_ref ROM values DIR M CC\n"; return 2; }
	try
	{
		const std::string cmd = _argv[2];
		if(cmd == "values" && _argc == 6)
			return values(_argv[1], _argv[3], static_cast<uint8_t>(std::stoi(_argv[4])), static_cast<uint8_t>(std::stoi(_argv[5])));
		if(cmd == "lcd") return lcd(_argv[1], _argv[3]);
		if(cmd == "render") return renderSpec(_argv[1], _argv[3]);
		if(cmd == "pages") return pages(_argv[1], _argv[3]);
		if(cmd == "panel") return panel(_argv[1], _argv[3]);
		if(cmd == "loop") return loop(_argv[1], _argv[3]);
		std::cerr << "unknown command\n";
		return 2;
	}
	catch(const std::exception& e)
	{
		std::cerr << "mm_ref: " << e.what() << '\n';
		return 1;
	}
}
