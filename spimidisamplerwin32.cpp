/*
 * Copyright (c) 2010-2016 Stephane Poirier
 *
 * stephane.poirier@oifii.org
 *
 * Stephane Poirier
 * 3532 rue Ste-Famille, #3
 * Montreal, QC, H2X 2L1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

////////////////////////////////////////////////////////////////
//nakedsoftware.org, spi@oifii.org or stephane.poirier@oifii.org
//xznn.com, xznnworldwide@gmail.com, carl w.b. poirier
//
//2014march18, creation of spimidisamplerwin32.cpp 
//2014march18, based on spimidisampler.cpp except it is a win32 app. 
//
//2014march18, showbytes() has not been revised, replace putchar()
//
//xznn.com, xznnworldwide@gmail.com, carl w.b. poirier
//nakedsoftware.org, spi@oifii.org or stephane.poirier@oifii.org
////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "spimidisamplerwin32.h"
#include "FreeImage.h"
#include <shellapi.h> //for CommandLineToArgW()
#include <mmsystem.h> //for timeSetEvent()
#include <stdio.h> //for swprintf()
#include <assert.h>
#include "spiwavsetlib.h"

#include "porttime.h"
#include "portmidi.h"
#include <map>

#include "portaudio.h"
#include "pa_asio.h"

//#define SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET		127
#define SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET			128

// Select sample format. 
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

// Global Variables:

CHAR pCHAR[1024];
WCHAR pWCHAR[1024];

PmStream* global_pPmStreamMIDIIN;      // midi input 
bool global_active = false;     // set when global_pPmStreamMIDIIN is ready for reading
bool global_inited = false;     // suppress printing during command line parsing 
int global_inputmidideviceid =  11; //alesis q49 midi port id (when midi yoke installed)
map<string,int> global_inputmididevicemap;

string global_instrumentnamepattern="";
string global_inputmididevicename = "Q49"; //"In From MIDI Yoke:  1", "In From MIDI Yoke:  2", ... , "In From MIDI Yoke:  8"

//string global_audiodevicename="E-MU ASIO"; //"Speakers (2- E-MU E-DSP Audio Processor (WDM))"
string global_audiodevicename="Speakers (2- E-MU E-DSP Audio P"; //"E-MU ASIO"
int global_outputAudioChannelSelectors[2]; 
map<string,int> global_devicemap;

Instrument* global_pInstrument=NULL;

#define MAX_LOADSTRING 100
FIBITMAP* global_dib;
HFONT global_hFont;
HWND global_hwnd=NULL;
MMRESULT global_timer=0;
#define MAX_GLOBALTEXT	4096
WCHAR global_text[MAX_GLOBALTEXT+1];
int global_x=100;
int global_y=200;
int global_xwidth=400;
int global_yheight=400;
BYTE global_alpha=200;
int global_fontheight=24;
int global_fontwidth=-1; //will be computed within WM_PAINT handler
BYTE global_fontcolor_r=255;
BYTE global_fontcolor_g=255;
BYTE global_fontcolor_b=255;
int global_staticalignment = 0; //0 for left, 1 for center and 2 for right
int global_staticheight=-1; //will be computed within WM_SIZE handler
int global_staticwidth=-1; //will be computed within WM_SIZE handler 
//spi, begin
int global_imageheight=-1; //will be computed within WM_SIZE handler
int global_imagewidth=-1; //will be computed within WM_SIZE handler 
//spi, end
int global_titlebardisplay=1; //0 for off, 1 for on
int global_acceleratoractive=1; //0 for off, 1 for on
int global_menubardisplay=0; //0 for off, 1 for on
FILE* global_pfile=NULL;
#define IDC_MAIN_EDIT	100
#define IDC_MAIN_STATIC	101

HINSTANCE hInst;								// current instance
//TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
//TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR szTitle[1024]={L"spimidisamplerwin32title"};					// The title bar text
TCHAR szWindowClass[1024]={L"spimidisamplerwin32class"};			// the main window class name

//new parameters
string global_begin="begin.ahk";
string global_end="end.ahk";

//#define StatusAddText StatusAddTextW

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Select sample format
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

#define MIDI_CODE_MASK  0xf0
#define MIDI_CHN_MASK   0x0f
//#define MIDI_REALTIME   0xf8
//  #define MIDI_CHAN_MODE  0xfa 
#define MIDI_OFF_NOTE   0x80
#define MIDI_ON_NOTE    0x90
#define MIDI_POLY_TOUCH 0xa0
#define MIDI_CTRL       0xb0
#define MIDI_CH_PROGRAM 0xc0
#define MIDI_TOUCH      0xd0
#define MIDI_BEND       0xe0

#define MIDI_SYSEX      0xf0
#define MIDI_Q_FRAME	0xf1
#define MIDI_SONG_POINTER 0xf2
#define MIDI_SONG_SELECT 0xf3
#define MIDI_TUNE_REQ	0xf6
#define MIDI_EOX        0xf7
#define MIDI_TIME_CLOCK 0xf8
#define MIDI_START      0xfa
#define MIDI_CONTINUE	0xfb
#define MIDI_STOP       0xfc
#define MIDI_ACTIVE_SENSING 0xfe
#define MIDI_SYS_RESET  0xff

#define MIDI_ALL_SOUND_OFF 0x78
#define MIDI_RESET_CONTROLLERS 0x79
#define MIDI_LOCAL	0x7a
#define MIDI_ALL_OFF	0x7b
#define MIDI_OMNI_OFF	0x7c
#define MIDI_OMNI_ON	0x7d
#define MIDI_MONO_ON	0x7e
#define MIDI_POLY_ON	0x7f

bool in_sysex = false;   // we are reading a sysex message 
bool done = false;       // when true, exit 
bool notes = true;       // show notes? 
bool controls = true;    // show continuous controllers 
bool bender = true;      // record pitch bend etc.? 
bool excldata = true;    // record system exclusive data? 
bool verbose = true;     // show text representation? 
bool realdata = true;    // record real time messages? 
bool clksencnt = true;   // clock and active sense count on 
bool chmode = true;      // show channel mode messages 
bool pgchanges = true;   // show program changes 
bool flush = false;	    // flush all pending MIDI data 

uint32_t filter = 0;            // remember state of midi filter 

uint32_t clockcount = 0;        // count of clocks 
uint32_t actsensecount = 0;     // cout of active sensing bytes 
uint32_t notescount = 0;        // #notes since last request 
uint32_t notestotal = 0;        // total #notes 

char val_format[] = "    Val %d\n";

/////////////////////////////////////////////////////////////////////////////
//               put_pitch
// Inputs:
//    int p: pitch number
// Effect: write out the pitch name for a given number
/////////////////////////////////////////////////////////////////////////////

static int put_pitch(int p)
{
    char result[8];
    static char *ptos[] = {
        "c", "cs", "d", "ef", "e", "f", "fs", "g",
        "gs", "a", "bf", "b"    };
    // note octave correction below 
    sprintf(result, "%s%d", ptos[p % 12], (p / 12) - 1);
    sprintf(pCHAR, "%s", result);StatusAddTextA(pCHAR);
    return strlen(result);
}


/////////////////////////////////////////////////////////////////////////////
//               showbytes
// Effect: print hex data, precede with newline if asked
/////////////////////////////////////////////////////////////////////////////

char nib_to_hex[] = "0123456789ABCDEF";

static void showbytes(PmMessage data, int len, bool newline)
{
    int count = 0;
    int i;

//    if (newline) {
//        putchar('\n');
//        count++;
//    } 
    for (i = 0; i < len; i++) 
	{
        putchar(nib_to_hex[(data >> 4) & 0xF]);
        putchar(nib_to_hex[data & 0xF]);
        count += 2;
        if (count > 72) 
		{
            putchar('.');
            putchar('.');
            putchar('.');
            break;
        }
        data >>= 8;
    }
    putchar(' ');
}

///////////////////////////////////////////////////////////////////////////////
//               output
// Inputs:
//    data: midi message buffer holding one command or 4 bytes of sysex msg
// Effect: format and print  midi data
///////////////////////////////////////////////////////////////////////////////
char vel_format[] = "    Vel %d\n";
static void output(PmMessage data)
{
    int command;    // the current command 
    int chan;   // the midi channel of the current event 
    int len;    // used to get constant field width 

    // printf("output data %8x; ", data); 

    command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    chan = Pm_MessageStatus(data) & MIDI_CHN_MASK;

    if (in_sysex || Pm_MessageStatus(data) == MIDI_SYSEX) {
#define sysex_max 16
        int i;
        PmMessage data_copy = data;
        in_sysex = true;
        // look for MIDI_EOX in first 3 bytes 
        // if realtime messages are embedded in sysex message, they will
        // be printed as if they are part of the sysex message
        //
        for (i = 0; (i < 4) && ((data_copy & 0xFF) != MIDI_EOX); i++) 
            data_copy >>= 8;
        if (i < 4) {
            in_sysex = false;
            i++; // include the EOX byte in output 
        }
        showbytes(data, i, verbose);
        if (verbose) 
		{
			sprintf(pCHAR, "System Exclusive\n");StatusAddTextA(pCHAR);
		}
    } else if (command == MIDI_ON_NOTE && Pm_MessageData2(data) != 0) {
        notescount++;
        if (notes) {
            showbytes(data, 3, verbose);
            if (verbose) 
			{
                sprintf(pCHAR, "NoteOn  Chan %2d Key %3d ", chan, Pm_MessageData1(data));StatusAddTextA(pCHAR);
                len = put_pitch(Pm_MessageData1(data));
                sprintf(pCHAR, vel_format + len, Pm_MessageData2(data));StatusAddTextA(pCHAR);
            }
        }
    } else if ((command == MIDI_ON_NOTE // && Pm_MessageData2(data) == 0
                || command == MIDI_OFF_NOTE) && notes) {
        showbytes(data, 3, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "NoteOff Chan %2d Key %3d ", chan, Pm_MessageData1(data));StatusAddTextA(pCHAR);
            len = put_pitch(Pm_MessageData1(data));
            sprintf(pCHAR, vel_format + len, Pm_MessageData2(data));StatusAddTextA(pCHAR);
        }
    } else if (command == MIDI_CH_PROGRAM && pgchanges) {
        showbytes(data, 2, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "  ProgChg Chan %2d Prog %2d\n", chan, Pm_MessageData1(data) + 1);StatusAddTextA(pCHAR);
        }
    } else if (command == MIDI_CTRL) {
               // controls 121 (MIDI_RESET_CONTROLLER) to 127 are channel
               // mode messages. 
        if (Pm_MessageData1(data) < MIDI_ALL_SOUND_OFF) {
            showbytes(data, 3, verbose);
            if (verbose) 
			{
                sprintf(pCHAR, "CtrlChg Chan %2d Ctrl %2d Val %2d\n",
                       chan, Pm_MessageData1(data), Pm_MessageData2(data));StatusAddTextA(pCHAR);
            }
        } else if (chmode) { // channel mode 
            showbytes(data, 3, verbose);
            if (verbose) {
                switch (Pm_MessageData1(data)) 
				{
                  case MIDI_ALL_SOUND_OFF:
                      sprintf(pCHAR, "All Sound Off, Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    break;
                  case MIDI_RESET_CONTROLLERS:
                    sprintf(pCHAR, "Reset All Controllers, Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    break;
                  case MIDI_LOCAL:
                    sprintf(pCHAR, "LocCtrl Chan %2d %s\n",
                            chan, Pm_MessageData2(data) ? "On" : "Off");StatusAddTextA(pCHAR);
                    break;
                  case MIDI_ALL_OFF:
                    sprintf(pCHAR, "All Off Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    break;
                  case MIDI_OMNI_OFF:
                    sprintf(pCHAR, "OmniOff Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    break;
                  case MIDI_OMNI_ON:
                    sprintf(pCHAR, "Omni On Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    break;
                  case MIDI_MONO_ON:
                    sprintf(pCHAR, "Mono On Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    if (Pm_MessageData2(data))
					{
                        sprintf(pCHAR, " to %d received channels\n", Pm_MessageData2(data));StatusAddTextA(pCHAR);
					}
                    else
					{
                        sprintf(pCHAR, " to all received channels\n");StatusAddTextA(pCHAR);
					}
                    break;
                  case MIDI_POLY_ON:
                    sprintf(pCHAR, "Poly On Chan %2d\n", chan);StatusAddTextA(pCHAR);
                    break;
                }
            }
        }
    } else if (command == MIDI_POLY_TOUCH && bender) {
        showbytes(data, 3, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "P.Touch Chan %2d Key %2d ", chan, Pm_MessageData1(data));StatusAddTextA(pCHAR);
            len = put_pitch(Pm_MessageData1(data));
            printf(val_format + len, Pm_MessageData2(data));
        }
    } else if (command == MIDI_TOUCH && bender) {
        showbytes(data, 2, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "  A.Touch Chan %2d Val %2d\n", chan, Pm_MessageData1(data));StatusAddTextA(pCHAR);
        }
    } else if (command == MIDI_BEND && bender) {
        showbytes(data, 3, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "P.Bend  Chan %2d Val %2d\n", chan,
                    (Pm_MessageData1(data) + (Pm_MessageData2(data)<<7)));StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_SONG_POINTER) {
        showbytes(data, 3, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Song Position %d\n",
                    (Pm_MessageData1(data) + (Pm_MessageData2(data)<<7)));StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_SONG_SELECT) {
        showbytes(data, 2, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Song Select %d\n", Pm_MessageData1(data));StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_TUNE_REQ) {
        showbytes(data, 1, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Tune Request\n");StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_Q_FRAME && realdata) {
        showbytes(data, 2, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Time Code Quarter Frame Type %d Values %d\n",
                    (Pm_MessageData1(data) & 0x70) >> 4, Pm_MessageData1(data) & 0xf);StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_START && realdata) {
        showbytes(data, 1, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Start\n");StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_CONTINUE && realdata) {
        showbytes(data, 1, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Continue\n");StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_STOP && realdata) {
        showbytes(data, 1, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    Stop\n");StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_SYS_RESET && realdata) {
        showbytes(data, 1, verbose);
        if (verbose) 
		{
            sprintf(pCHAR, "    System Reset\n");StatusAddTextA(pCHAR);
        }
    } else if (Pm_MessageStatus(data) == MIDI_TIME_CLOCK) {
        if (clksencnt) clockcount++;
        else if (realdata) {
            showbytes(data, 1, verbose);
            if (verbose) 
			{
                sprintf(pCHAR, "    Clock\n");StatusAddTextA(pCHAR);
            }
        }
    } else if (Pm_MessageStatus(data) == MIDI_ACTIVE_SENSING) {
        if (clksencnt) actsensecount++;
        else if (realdata) {
            showbytes(data, 1, verbose);
            if (verbose) 
			{
                sprintf(pCHAR, "    Active Sensing\n");StatusAddTextA(pCHAR);
            }
        }
    } else showbytes(data, 3, verbose);
    fflush(stdout);
}

void receive_poll(PtTimestamp timestamp, void *userData)
{
    PmEvent event;
    int count; 
    if (!global_active) return;
    while ((count = Pm_Read(global_pPmStreamMIDIIN, &event, 1))) 
	{
        if (count == 1) 
		{
			//1) output message
			output(event.message);
			//2) play note
			Instrument* pInstrument = (Instrument*) userData;
			int msgstatus = Pm_MessageStatus(event.message);
			//if(msgstatus==MIDI_ON_NOTE)
			if(msgstatus>=MIDI_ON_NOTE && msgstatus<(MIDI_ON_NOTE+16) && Pm_MessageData2(event.message)!=0)
			{
				int notenumber = Pm_MessageData1(event.message);
				WavSet* pWavSet = pInstrument->GetWavSetFromMidiNoteNumber(notenumber);
				assert(pWavSet);
				//pWavSet->Play(USING_SOX, 1.0f); //warning, don't know how to control play time length with sox
				//pWavSet->Play(USING_SPIPLAY, 1.0f);
				//pWavSet->Play(USING_SPISPECTRUMPLAY, 1.0f);
				//pWavSet->Play(USING_SPIPLAYSTREAM, 1.0f);
				pWavSet->Play(USING_SPIPLAYX, 1.0f);

			}
		}
        else            
		{
			//printf(Pm_GetErrorText((PmError)count)); //spi a cast as (PmError)
			sprintf(pCHAR, Pm_GetErrorText((PmError)count));StatusAddTextA(pCHAR);
		}
    }
}

void CALLBACK StartGlobalProcess(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	//WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight);
	global_pfile = fopen("output.txt","w");
	WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment, global_pfile);

	//testing start /b, it does not work, like if /b has not effect
	//system("start /b c:\\app-bin\\sox\\sox.exe -q \"d:\\temp\\test.wav\" -d trim 0 10.0");
	//testing ShellExecute(), it works
	//ShellExecute(NULL, L"open", L"c:\\app-bin\\sox\\sox.exe", L"-q \"d:\\temp\\test.wav\" -d trim 0 10.0", NULL, 0);

	//////////////////////////
	//initialize random number
	//////////////////////////
	srand((unsigned)time(0));


	//////////////////////
	//initialize portaudio
	//////////////////////
	PaStreamParameters outputParameters;
    PaStream* stream;
    PaError myPaError;

    myPaError = Pa_Initialize();
    if( myPaError != paNoError ) //goto error;
	{
		sprintf(pCHAR, "An error occured while using the portaudio stream\n" );StatusAddText(pCHAR);
		sprintf(pCHAR, "Error number: %d\n", myPaError );StatusAddText(pCHAR);
		sprintf(pCHAR, "Error message: %s\n", Pa_GetErrorText( myPaError ) );StatusAddTextA(pCHAR);
		return;
	}

	////////////////////////
	//audio device selection
	////////////////////////
	const PaDeviceInfo* pPaDeviceInfo;
    int numPaDevices = Pa_GetDeviceCount();
    for( int i=0; i<numPaDevices; i++ )
    {
        pPaDeviceInfo = Pa_GetDeviceInfo( i );
		string devicenamestring = pPaDeviceInfo->name;
		global_devicemap.insert(pair<string,int>(devicenamestring,i));
	}

	int deviceid = Pa_GetDefaultOutputDevice(); // default output device 
	map<string,int>::iterator itmap;
	itmap = global_devicemap.find(global_audiodevicename);
	if(itmap!=global_devicemap.end())
	{
		deviceid = (*itmap).second;
		sprintf(pCHAR, "%s maps to %d\n", global_audiodevicename.c_str(), deviceid);StatusAddText(pCHAR);
		pPaDeviceInfo = Pa_GetDeviceInfo(deviceid);
		//deviceInfo->maxInputChannels
		assert(global_outputAudioChannelSelectors[0]<pPaDeviceInfo->maxOutputChannels);
		assert(global_outputAudioChannelSelectors[1]<pPaDeviceInfo->maxOutputChannels);
	}
	else
	{
		for(itmap=global_devicemap.begin(); itmap!=global_devicemap.end(); itmap++)
		{
			sprintf(pCHAR, "%s maps to %d\n", (*itmap).first.c_str(), (*itmap).second);StatusAddText(pCHAR);
		}
		//Pa_Terminate();
		//return -1;
		sprintf(pCHAR, "error, audio device not found, will use default\n");StatusAddText(pCHAR);
		deviceid = Pa_GetDefaultOutputDevice();
	}


	//outputParameters.device = Pa_GetDefaultOutputDevice(); // default output device 
	outputParameters.device = deviceid; // default output device 
	if (outputParameters.device == paNoDevice) 
	{
		sprintf(pCHAR, "Error: No default output device.\n");StatusAddText(pCHAR);
		sprintf(pCHAR, "An error occured while using the portaudio stream\n" );StatusAddText(pCHAR);
		sprintf(pCHAR, "Error number: %d\n", myPaError );StatusAddText(pCHAR);
		sprintf(pCHAR, "Error message: %s\n", Pa_GetErrorText( myPaError ) );StatusAddTextA(pCHAR);
		return;
	}
	outputParameters.channelCount = 2;//pWavSet->numChannels;
	outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	//outputParameters.hostApiSpecificStreamInfo = NULL;
	//Use an ASIO specific structure. WARNING - this is not portable. 
	PaAsioStreamInfo asioOutputInfo;
	asioOutputInfo.size = sizeof(PaAsioStreamInfo);
	asioOutputInfo.hostApiType = paASIO;
	asioOutputInfo.version = 1;
	asioOutputInfo.flags = paAsioUseChannelSelectors;
	asioOutputInfo.channelSelectors = global_outputAudioChannelSelectors;
	//outputChannelSelectors[0] = 0; // ASIO device channel 1 (left)
	//outputChannelSelectors[1] = 1; // ASIO device channel 2 (right)
	if(deviceid==Pa_GetDefaultOutputDevice())
	{
		outputParameters.hostApiSpecificStreamInfo = NULL;
	}
	else if(Pa_GetHostApiInfo(Pa_GetDeviceInfo(deviceid)->hostApi)->type == paASIO) 
	{
		outputParameters.hostApiSpecificStreamInfo = &asioOutputInfo;
	}
	else if(Pa_GetHostApiInfo(Pa_GetDeviceInfo(deviceid)->hostApi)->type == paWDMKS) 
	{
		/*
		//Use an WDMKS specific structure. WARNING - this is not portable. 
		PaWDMKSStreamInfo wdmksOutputInfo;
		asioOutputInfo.size = sizeof(PaWDMKSStreamInfo);
		asioOutputInfo.hostApiType = paWDMKS;
		asioOutputInfo.version = 1;
		asioOutputInfo.flags = paAsioUseChannelSelectors;
		//outputChannelSelectors[0] = 0; // ASIO device channel 1 (left)
		//outputChannelSelectors[1] = 1; // ASIO device channel 2 (right)
		outputParameters.hostApiSpecificStreamInfo = &wdmksOutputInfo;
		*/
		outputParameters.hostApiSpecificStreamInfo = NULL;
	}
	else
	{
		//assert(false);
		outputParameters.hostApiSpecificStreamInfo = NULL;
	}


	////////////////////////
	//populate InstrumentSet
	////////////////////////
	//InstrumentSet* pInstrumentSet=new InstrumentSet;
	//Instrument* pInstrument = new Instrument;
	global_pInstrument = new Instrument;
	//pInstrument->CreateFromName("piano", SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET);
	//pInstrument->CreateFromName("violin", SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET);
	//global_pInstrument->CreateFromWavFolder("C:\\Program Files (x86)\\Native Instruments\\Sample Libraries\\Kontakt 3 Library\\Orchestral\\Z - Samples\\03 Cello ensemble - 8\\VC-8_mV_0aT1-legsus_p", SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET);
	//global_pInstrument->CreateFromWavFilenamesFilter(NULL, SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET); //selects instrument randomly out of the "wavfolder*.txt" files
	if(global_instrumentnamepattern.empty())
	{
		sprintf(pCHAR, "Generating sine wav for all notes ... \n");StatusAddTextA(pCHAR);
		//global_pInstrument->CreateWavSynth(INSTRUMENT_SYNTH_SAWWAV);
		global_pInstrument->CreateWavSynth(INSTRUMENT_SYNTH_SINWAV);
		//WriteWavFiles() so sox can play them
		global_pInstrument->WriteWavFiles(INSTRUMENT_TEMPFOLDER);
		sprintf(pCHAR, "Done.\n");StatusAddTextA(pCHAR);
	}
	//spi, 2014nov02, begin
	//else if()
	else
	{
		global_pInstrument->CreateFromWavFolder(global_instrumentnamepattern.c_str(), SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET); //selects instrument out of the "wavfolder*.txt" files (based on the supplied instrument name pattern)
	}
	/*
	else
	{
		global_pInstrument->CreateFromWavFilenamesFilter(global_instrumentnamepattern.c_str(), SPIMIDISAMPLER_INSTRUMENT_MAXNUMBEROFWAVSET); //selects instrument out of the "wavfolder*.txt" files (based on the supplied instrument name pattern)
	}
	*/
	//spi, 2014nov02, end
	global_pInstrument->Play(&outputParameters, INSTRUMENT_WAVSETALLATONCE);


	/////////////////////
	//initialize portmidi
	/////////////////////
    PmError err;
	Pm_Initialize(); 

	/////////////////////////////
	//input midi device selection
	/////////////////////////////
	const PmDeviceInfo* deviceInfo;
    int numDevices = Pm_CountDevices();
    for( int i=0; i<numDevices; i++ )
    {
        deviceInfo = Pm_GetDeviceInfo( i );
		if (deviceInfo->input)
		{
			string devicenamestring = deviceInfo->name;
			global_inputmididevicemap.insert(pair<string,int>(devicenamestring,i));
		}
	}
	map<string,int>::iterator it;
	it = global_inputmididevicemap.find(global_inputmididevicename);
	if(it!=global_inputmididevicemap.end())
	{
		global_inputmidideviceid = (*it).second;
		sprintf(pCHAR, "%s maps to %d\n", global_inputmididevicename.c_str(), global_inputmidideviceid);StatusAddTextA(pCHAR);
		deviceInfo = Pm_GetDeviceInfo(global_inputmidideviceid);
	}
	else
	{
		assert(false);
		for(it=global_inputmididevicemap.begin(); it!=global_inputmididevicemap.end(); it++)
		{
			sprintf(pCHAR, "%s maps to %d\n", (*it).first.c_str(), (*it).second);StatusAddTextA(pCHAR);
		}
		swprintf(pWCHAR, L"input midi device not found\n");StatusAddText(pWCHAR);
		return;
	}

    // use porttime callback to empty midi queue and print 
    Pt_Start(1, receive_poll, global_pInstrument); //Pt_Start(1, receive_poll, 0);
    // list device information 
    swprintf(pWCHAR, L"MIDI input devices:\n");StatusAddText(pWCHAR);
    for (int i = 0; i < Pm_CountDevices(); i++) 
	{
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info->input) 
		{
			sprintf(pCHAR, "%d: %s, %s\n", i, info->interf, info->name);StatusAddTextA(pCHAR);
		}
    }
    //inputmididevice = get_number("Type input device number: ");
	swprintf(pWCHAR, L"device %d selected\n", global_inputmidideviceid);StatusAddText(pWCHAR);

    err = Pm_OpenInput(&global_pPmStreamMIDIIN, global_inputmidideviceid, NULL, 512, NULL, NULL);
    if (err) 
	{
        sprintf(pCHAR, Pm_GetErrorText(err));StatusAddTextA(pCHAR);
        Pt_Stop();
		//Terminate();
        //mmexit(1);
		return;
    }
    Pm_SetFilter(global_pPmStreamMIDIIN, filter);
    global_inited = true; // now can document changes, set filter 
    swprintf(pWCHAR, L"spimidisamplerwin32 ready.\n");StatusAddText(pWCHAR);
    global_active = true;

	//PostMessage(global_hwnd, WM_DESTROY, 0, 0);
}




PCHAR*
    CommandLineToArgvA(
        PCHAR CmdLine,
        int* _argc
        )
    {
        PCHAR* argv;
        PCHAR  _argv;
        ULONG   len;
        ULONG   argc;
        CHAR   a;
        ULONG   i, j;

        BOOLEAN  in_QM;
        BOOLEAN  in_TEXT;
        BOOLEAN  in_SPACE;

        len = strlen(CmdLine);
        i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

        argv = (PCHAR*)GlobalAlloc(GMEM_FIXED,
            i + (len+2)*sizeof(CHAR));

        _argv = (PCHAR)(((PUCHAR)argv)+i);

        argc = 0;
        argv[argc] = _argv;
        in_QM = FALSE;
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        i = 0;
        j = 0;

        while( a = CmdLine[i] ) {
            if(in_QM) {
                if(a == '\"') {
                    in_QM = FALSE;
                } else {
                    _argv[j] = a;
                    j++;
                }
            } else {
                switch(a) {
                case '\"':
                    in_QM = TRUE;
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    in_SPACE = FALSE;
                    break;
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    if(in_TEXT) {
                        _argv[j] = '\0';
                        j++;
                    }
                    in_TEXT = FALSE;
                    in_SPACE = TRUE;
                    break;
                default:
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    _argv[j] = a;
                    j++;
                    in_SPACE = FALSE;
                    break;
                }
            }
            i++;
        }
        _argv[j] = '\0';
        argv[argc] = NULL;

        (*_argc) = argc;
        return argv;
    }

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	//LPWSTR *szArgList;
	LPSTR *szArgList;
	int nArgs;
	int i;

	//szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	szArgList = CommandLineToArgvA(GetCommandLineA(), &nArgs);
	if( NULL == szArgList )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return FALSE;
	}
	LPWSTR *szArgListW;
	int nArgsW;
	szArgListW = CommandLineToArgvW(GetCommandLineW(), &nArgsW);
	if( NULL == szArgListW )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return FALSE;
	}

	if(nArgs>1)
	{
		global_instrumentnamepattern=szArgList[1];
	}
	//int inputmididevice =  11; //alesis q49 midi port id (when midi yoke installed)
	//int inputmididevice =  1; //midi yoke 1 (when midi yoke installed)
	if(nArgs>2)
	{
		//inputmididevice=atoi(argv[2]);
		global_inputmididevicename=szArgList[2]; //"Q49", "In From MIDI Yoke:  1", "In From MIDI Yoke:  2", ... , "In From MIDI Yoke:  8"
	}
	if(nArgs>3)
	{
		global_x = atoi(szArgList[3]);
	}
	if(nArgs>4)
	{
		global_y = atoi(szArgList[4]);
	}
	if(nArgs>5)
	{
		global_xwidth = atoi(szArgList[5]);
	}
	if(nArgs>6)
	{
		global_yheight = atoi(szArgList[6]);
	}
	if(nArgs>7)
	{
		global_alpha = atoi(szArgList[7]);
	}
	if(nArgs>8)
	{
		global_titlebardisplay = atoi(szArgList[8]);
	}
	if(nArgs>9)
	{
		global_menubardisplay = atoi(szArgList[9]);
	}
	if(nArgs>10)
	{
		global_acceleratoractive = atoi(szArgList[10]);
	}
	if(nArgs>11)
	{
		global_fontheight = atoi(szArgList[11]);
	}
	if(nArgs>12)
	{
		global_fontcolor_r = atoi(szArgList[12]);
	}
	if(nArgs>13)
	{
		global_fontcolor_g = atoi(szArgList[13]);
	}
	if(nArgs>14)
	{
		global_fontcolor_b = atoi(szArgList[14]);
	}
	if(nArgs>15)
	{
		global_staticalignment = atoi(szArgList[15]);
	}
	//new parameters
	if(nArgs>16)
	{
		wcscpy(szWindowClass, szArgListW[16]); 
	}
	if(nArgs>17)
	{
		wcscpy(szTitle, szArgListW[17]); 
	}
	if(nArgs>18)
	{
		global_begin = szArgList[18]; 
	}
	if(nArgs>19)
	{
		global_end = szArgList[19]; 
	}
	if(nArgs>20)
	{
		global_audiodevicename = szArgList[20];
	}
	global_outputAudioChannelSelectors[0] = 6; // on emu patchmix ASIO device channel 15 (left)
	global_outputAudioChannelSelectors[1] = 7; // on emu patchmix ASIO device channel 16 (right)
	if(nArgs>21)
	{
		global_outputAudioChannelSelectors[0]=atoi(szArgList[21]); //0 for first asio channel (left) or 2, 4, 6 and 8 for spi (maxed out at 10 asio output channel)
	}
	if(nArgs>22)
	{
		global_outputAudioChannelSelectors[1]=atoi(szArgList[22]); //1 for second asio channel (right) or 3, 5, 7 and 9 for spi (maxed out at 10 asio output channel)
	}

	LocalFree(szArgList);
	LocalFree(szArgListW);

	int nShowCmd = false;
	//ShellExecuteA(NULL, "open", "begin.bat", "", NULL, nShowCmd);
	ShellExecuteA(NULL, "open", global_begin.c_str(), "", NULL, nCmdShow);

	//testing start /b
	//system("start /B c:\\app-bin\\sox\\sox.exe -q \"d:\\temp\\test.wav\" -d trim 0 10.0");


	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	//LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	//LoadString(hInstance, IDC_SPIWAVWIN32, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	if(global_acceleratoractive)
	{
		hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SPIWAVWIN32));
	}
	else
	{
		hAccelTable = NULL;
	}
	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	//wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SPIWAVWIN32));
	wcex.hIcon			= (HICON)LoadImage(NULL, L"background_32x32x16.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);

	if(global_menubardisplay)
	{
		wcex.lpszMenuName = MAKEINTRESOURCE(IDC_SPIWAVWIN32); //original with menu
	}
	else
	{
		wcex.lpszMenuName = NULL; //no menu
	}
	wcex.lpszClassName	= szWindowClass;
	//wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hIconSm		= (HICON)LoadImage(NULL, L"background_16x16x16.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	global_dib = FreeImage_Load(FIF_JPEG, "background.jpg", JPEG_DEFAULT);

	//global_hFont=CreateFontW(32,0,0,0,FW_BOLD,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");
	global_hFont=CreateFontW(global_fontheight,0,0,0,FW_NORMAL,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");

	if(global_titlebardisplay)
	{
		hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, //original with WS_CAPTION etc.
			global_x, global_y, global_xwidth, global_yheight, NULL, NULL, hInstance, NULL);
	}
	else
	{
		hWnd = CreateWindow(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE, //no WS_CAPTION etc.
			global_x, global_y, global_xwidth, global_yheight, NULL, NULL, hInstance, NULL);
	}
	if (!hWnd)
	{
		return FALSE;
	}
	global_hwnd = hWnd;

	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hWnd, 0, global_alpha, LWA_ALPHA);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	HGDIOBJ hOldBrush;
	HGDIOBJ hOldPen;
	int iOldMixMode;
	COLORREF crOldBkColor;
	COLORREF crOldTextColor;
	int iOldBkMode;
	HFONT hOldFont, hFont;
	TEXTMETRIC myTEXTMETRIC;

	switch (message)
	{
	case WM_CREATE:
		{
			//HWND hStatic = CreateWindowEx(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,  
			HWND hStatic = CreateWindowEx(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | global_staticalignment, 
				0, 100, 100, 100, hWnd, (HMENU)IDC_MAIN_STATIC, GetModuleHandle(NULL), NULL);
			if(hStatic == NULL)
				MessageBox(hWnd, L"Could not create static text.", L"Error", MB_OK | MB_ICONERROR);
			SendMessage(hStatic, WM_SETFONT, (WPARAM)global_hFont, MAKELPARAM(FALSE, 0));



			global_timer=timeSetEvent(1000,25,(LPTIMECALLBACK)&StartGlobalProcess,0,TIME_ONESHOT);
		}
		break;
	case WM_SIZE:
		{
			RECT rcClient;

			GetClientRect(hWnd, &rcClient);
			/*
			HWND hEdit = GetDlgItem(hWnd, IDC_MAIN_EDIT);
			SetWindowPos(hEdit, NULL, 0, 0, rcClient.right/2, rcClient.bottom/2, SWP_NOZORDER);
			*/
			HWND hStatic = GetDlgItem(hWnd, IDC_MAIN_STATIC);
			global_staticwidth = rcClient.right - 0;
			//global_staticheight = rcClient.bottom-(rcClient.bottom/2);
			global_staticheight = rcClient.bottom - 0;

			//spi, begin
			global_imagewidth = rcClient.right - 0;
			global_imageheight = rcClient.bottom - 0; 
			WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment, global_pfile);
			//spi, end
			//SetWindowPos(hStatic, NULL, 0, rcClient.bottom/2, global_staticwidth, global_staticheight, SWP_NOZORDER);
			SetWindowPos(hStatic, NULL, 0, 0, global_staticwidth, global_staticheight, SWP_NOZORDER);
		}
		break;
	case WM_CTLCOLOREDIT:
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetTextColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
			return (INT_PTR)::GetStockObject(NULL_PEN);
		}
		break;
	case WM_CTLCOLORSTATIC:
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			//SetTextColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
			SetTextColor((HDC)wParam, RGB(global_fontcolor_r, global_fontcolor_g, global_fontcolor_b));
			return (INT_PTR)::GetStockObject(NULL_PEN);
		}
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		//spi, begin
		
		SetStretchBltMode(hdc, COLORONCOLOR);
		
		StretchDIBits(hdc, 0, 0, global_imagewidth, global_imageheight,
						0, 0, FreeImage_GetWidth(global_dib), FreeImage_GetHeight(global_dib),
						FreeImage_GetBits(global_dib), FreeImage_GetInfo(global_dib), DIB_RGB_COLORS, SRCCOPY);
		
		//spi, end
		hOldBrush = SelectObject(hdc, (HBRUSH)GetStockObject(GRAY_BRUSH));
		hOldPen = SelectObject(hdc, (HPEN)GetStockObject(WHITE_PEN));
		//iOldMixMode = SetROP2(hdc, R2_MASKPEN);
		iOldMixMode = SetROP2(hdc, R2_MERGEPEN);
		//Rectangle(hdc, 100, 100, 200, 200);

		crOldBkColor = SetBkColor(hdc, RGB(0xFF, 0x00, 0x00));
		crOldTextColor = SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
		iOldBkMode = SetBkMode(hdc, TRANSPARENT);
		//hFont=CreateFontW(70,0,0,0,FW_BOLD,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");
		//hOldFont=(HFONT)SelectObject(hdc,global_hFont);
		hOldFont=(HFONT)SelectObject(hdc,global_hFont);
		GetTextMetrics(hdc, &myTEXTMETRIC);
		global_fontwidth = myTEXTMETRIC.tmAveCharWidth;
		//TextOutW(hdc, 100, 100, L"test string", 11);

		SelectObject(hdc, hOldBrush);
		SelectObject(hdc, hOldPen);
		SetROP2(hdc, iOldMixMode);
		SetBkColor(hdc, crOldBkColor);
		SetTextColor(hdc, crOldTextColor);
		SetBkMode(hdc, iOldBkMode);
		SelectObject(hdc,hOldFont);
		//DeleteObject(hFont);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		{
			//terminate portmidi
			global_active = false;
			Pm_Close(global_pPmStreamMIDIIN);
			Pt_Stop();
			Pm_Terminate();
			//terminate portaudio
			Pa_Terminate();
			//delete all memory allocations
			if(global_pInstrument) delete global_pInstrument;
			//close file
			if(global_pfile) fclose(global_pfile);
			//terminate wavset library
			WavSetLib_Terminate();
			//terminate win32 app.
			if (global_timer) timeKillEvent(global_timer);
			FreeImage_Unload(global_dib);
			DeleteObject(global_hFont);

			int nShowCmd = false;
			//ShellExecuteA(NULL, "open", "end.bat", "", NULL, nShowCmd);
			ShellExecuteA(NULL, "open", global_end.c_str(), "", NULL, 0);
			PostQuitMessage(0);
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
