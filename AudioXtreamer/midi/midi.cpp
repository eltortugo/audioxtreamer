#include "stdafx.h"
#include "midi.h"

#include <intrin.h>


typedef struct MIDI_PORT* PMIDI_PORT;

typedef void (CALLBACK* DATA_CB)(PMIDI_PORT midiPort, LPBYTE midiDataBytes, DWORD length, DWORD_PTR dwCallbackInstance);

typedef PMIDI_PORT(CALLBACK* CREATE_PORT)(LPCWSTR portName, DATA_CB callback, DWORD_PTR dwCallbackInstance, DWORD maxSysexLength, DWORD flags);
typedef void(CALLBACK* CLOSE_PORT)(PMIDI_PORT midiPort);
typedef BOOL(CALLBACK* SEND_DATA)(PMIDI_PORT midiPort, LPBYTE midiDataBytes, DWORD length);


struct _midiBknd
{
  HMODULE module;
  CREATE_PORT create_port;
  CLOSE_PORT close_port;
  SEND_DATA send_data;

} lib;

static FARPROC get_proc_addr(LPCSTR api_name)
{
  FARPROC api_ptr = GetProcAddress(lib.module, api_name);

  if (api_ptr == NULL) {
    LOGN("MidiHelper API %s not found: 0x%08X", api_name, GetLastError());
  }
  return api_ptr;
}

void unload_lib(void)
{
  if (lib.module != NULL) {
    FreeLibrary(lib.module);
    lib.module = NULL;
  }
}

bool load_lib()
{
#if defined _M_IX86
  lib.module = LoadLibraryA("teVirtualMidi");
#elif defined _M_X64
  lib.module = LoadLibraryA("teVirtualMidi64");
#endif*/
  
  if (lib.module == NULL) {
    LOGN("Failed to load MidiHelper.dll: 0x%08X", GetLastError());
    return false;
  }

  lib.create_port = (CREATE_PORT)get_proc_addr("virtualMIDICreatePortEx2");
  if (lib.create_port == NULL)
    goto error_unload;

  lib.close_port = (CLOSE_PORT)get_proc_addr("virtualMIDIClosePort");
  if (lib.close_port == NULL)
    goto error_unload;

  lib.send_data = (SEND_DATA)get_proc_addr("virtualMIDISendData");
  if (lib.send_data == NULL)
    goto error_unload;

  return true;
error_unload:
  FreeLibrary(lib.module);
  lib.module = NULL;
  return false;

}

#define MAX_SYSEX_BUFFER	65535

class midiport
{
public:
  midiport();
  ~midiport();
  void cb(struct MIDI_PORT* midiPort, LPBYTE midiDataBytes, DWORD length);
  bool read(uint8_t(&data)[4], uint8_t& size);

  static void CALLBACK scb(struct MIDI_PORT* p, LPBYTE d, DWORD l, DWORD_PTR i) {
    if (i)
      ((midiport*)i)->cb(p, d, l);
  }
  friend class MidiIO;

private:
  void open(uint8_t idx);
  void close();

  struct MIDI_PORT* port;
  uint16_t rxlen;
  uint8_t data[MAX_SYSEX_BUFFER];
  volatile DWORD txlen;
  LPBYTE txbuff;
  HANDLE hevent;
};

midiport::midiport()
  : port(nullptr)
  , rxlen(0)
  , txlen(0)
  , txbuff(nullptr)
  , hevent(NULL)
  , data{ 0,0,0,0 }
{}


midiport::~midiport()
{
}

void midiport::cb(MIDI_PORT* midiPort, LPBYTE midiDataBytes, DWORD length)
{
  if (midiPort == port && midiDataBytes && length)
  {
    txbuff = midiDataBytes;
    _InterlockedExchange(&txlen, length);
    do {
      //wait till the reader has read it all
      DWORD r = WaitForSingleObject(hevent, 100);
      if (r != WAIT_OBJECT_0)
        break;
    } while (_InterlockedExchange(&txlen, txlen) > 0);
  }
}

//called by the reader every ms so technically we throttle to 30kbps instead of 31k25
bool midiport::read(uint8_t(&data)[4], uint8_t& size)
{
  int c = 0;
  int len = _InterlockedExchange(&txlen, txlen);
  if (len > 0) //the callback is waiting so we can proceed
  {
    for (; len > 0 && c < 3; len--, c++)
      data[c] = *txbuff++;

    size = c;
    _InterlockedExchange(&txlen, txlen - c);
    SetEvent(hevent);
  }
  else
    size = 0;
  return c > 0;
}

void midiport::open(uint8_t idx)
{
  WCHAR str[32];
  swprintf_s(str, L"YMH01xUSB MIDI(%u)", idx + 1);
  port = lib.create_port(str, &scb, (DWORD_PTR)this, MAX_SYSEX_BUFFER, 1);
  rxlen = 0;
  hevent = CreateEvent(nullptr, false, false, nullptr);
}


void midiport::close()
{
  if(port)
    lib.close_port(port);
  if(hevent)
    CloseHandle(hevent);
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

MidiIO::MidiIO()
: ports(nullptr)
{
  if (!load_lib())
    return;


  ports = new midiport[8];
  //open the ports
  for (int i = 0; i < 8; i++) {
    ports[i].open(i);
  }
}


MidiIO::~MidiIO()
{
  if (ports) {
    for (int i = 0; i < 8; i++)
      ports[i].close();

    delete[]ports;
  }

  unload_lib();
}

void MidiIO::Init()
{
  if(ports != nullptr)
    for (int i = 0; i < 8; i++)
      ports[i].rxlen = 0;
}


static uint32_t msgnr = 0;

typedef  uint8_t MidiData[8];


void MidiIO::MidiIn(uint8_t * ptr)
{
  if (ports == nullptr || *ptr == 0)
    return;

  uint8_t(&v)[8] = *(MidiData*)ptr;

  for (int idx = 0; idx < 7; idx++)
  {
    if (v[0] & (1 << idx))
    {
      uint8_t data = v[idx + 1];

      if (ports[idx].port != 0)
      {
        LPBYTE bytes = ports[idx].data;
        uint16_t& rxlen = ports[idx].rxlen;

        switch (rxlen) {

        case 0:
          switch (data)
          {
          case 0xf4:
          case 0xf5:
          case 0xf6:
            //case 0xf7
          case 0xf8:
          case 0xf9:
          case 0xfa:
          case 0xfb:
          case 0xfc:
          case 0xfd:
          case 0xfe:
          case 0xff:
            lib.send_data(ports[idx].port, &data, 1);
            LOGN("% 6d(%u) %02x\n", msgnr++, idx + 1, data);
            break;
          default:
            if (data & 0x80) { //save the status
              if (data == 0xf7)
                break;
              bytes[0] = data;
              rxlen++;
            }
            else { //running status 
              switch (bytes[0] & 0xf0) {//ignore the channel so we can switch
              case 0xc0:
              case 0xd0:
                bytes[1] = data;
                lib.send_data(ports[idx].port, bytes, 2);
                LOGN("% 6d(%u) %02x %02x\n", msgnr++, idx + 1, bytes[0], bytes[1]);
                rxlen = 0;
                break;

              case 0x80:
              case 0x90:
              case 0xa0:
              case 0xb0:
              case 0xe0:
                bytes[1] = data;
                rxlen = 2;
                break;
              default:
                break;
              }
            }
            break;
          }
          break;

        case 1:
          bytes[1] = data;
          if ((bytes[0] & 0xf0) == 0xc0
            || (bytes[0] & 0xf0) == 0xd0
            || bytes[0] == 0xf1
            || bytes[0] == 0xf3)
          {
            if ((data & 0x80) == 0) {
              lib.send_data(ports[idx].port, bytes, 2);
              LOGN("% 6d(%u) %02x %02x\n", msgnr++, idx + 1, bytes[0], bytes[1]);
            }
            rxlen = 0;
          }
          else {
            if ((data & 0x80) == 0)
              rxlen = 2;
            else
              rxlen = 0;
          }
          break;

        case 2:
          bytes[2] = data;
          switch (bytes[0] & 0xf0)
          {
          case 0x80:
          case 0x90:
          case 0xa0:
          case 0xb0:
          case 0xe0:
            if ((data & 0x80) == 0) {
              lib.send_data(ports[idx].port, bytes, 3);
              LOGN("% 6d(%u) %02x %02x %02x\n", msgnr++, idx + 1, bytes[0], bytes[1], bytes[2]);
            }
            rxlen = 0;
          default:
            if (bytes[0] == 0xf0) {
              bytes[2] = data;
              rxlen = 3;
            }
            break;
          }
          break;

        default:
          if (rxlen < MAX_SYSEX_BUFFER && bytes[0] == 0xf0)
          {
            bytes[rxlen++] = data;
            if (data == 0xf7)
            {
              lib.send_data(ports[idx].port, bytes, rxlen);
              LOGN("% 6d(%u) %02x %02x %02x sysx len %u\n", msgnr++, idx + 1, bytes[0], bytes[1], bytes[2], rxlen);
              bytes[0] = 0;
              rxlen = 0;
            }
          }
          break;
        }
      }
    }
  }
}

uint8_t MidiIO::MidiOut(uint8_t* ptr)
{
  if (nullptr == ports || nullptr == ptr)
    return 0;

  uint8_t data[8][4];
  uint8_t sizes[8];

  bool ret = false;
  for (int i = 0; i < 8; i++)
    ret |= ports[i].read(data[i], sizes[i]);

  if (ret)
  {

    //midi packet header 4bytes
    *ptr++ = 0x96;
    *ptr++ = 0x69;
    *ptr++ = 0x69;
    *ptr++ = 0x96;

    // sizes 2 bytes
    *ptr++ = (sizes[0] & 0x3)
      | ((sizes[1] & 0x3) << 2)
      | ((sizes[2] & 0x3) << 4)
      | ((sizes[3] & 0x3) << 6);
    *ptr++ = ((sizes[4] & 0x3))
      | ((sizes[5] & 0x3) << 2)
      | ((sizes[6] & 0x3) << 4)
      | ((sizes[7] & 0x3) << 6);

    //data 24
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 4; ++j)
      {
        *ptr++ = data[j * 2][i];
        *ptr++ = data[(j * 2) + 1][i];
      }

    return 30;
  }
  else
    return 0;
}

