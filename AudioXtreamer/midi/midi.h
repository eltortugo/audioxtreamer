#pragma once

struct MIDI_PORT;
class midiport;
class MidiIO
{
public:
  MidiIO();
  ~MidiIO();

  void Init();
  void MidiIn(uint8_t *ptr);
  uint8_t MidiOut(uint8_t* ptr);
 

private:

  friend class midiport;
  midiport *ports;
};
