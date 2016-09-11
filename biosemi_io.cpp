#include <cstdint>
#include <cstring>
#include <iostream>
#include <exception>
#include <vector>
#include <time.h>
#include <fstream>
#include <string>
#include <iterator>
#include "chunk_transform.h"
#include "biosemi_io.h"
#include "Resampler.h"
//const char *dllpath = "/usr/lib/liblabview_dll.0.0.1.dylib";
//const string test_file_path = "/usr/tmp/chunk_timing_test.txt";
//std::ofstream test_file(test_file_path.c_str());
//biosemi_io::chunk_t raw_chunk;
const double coeffs_4x_lp[] = {0, 0, 0, 0, 5.108107e-19, 1.294707e-03, 3.006461e-03, 3.096457e-03, -2.924656e-18, -5.520185e-03,
	-9.920641e-03, -8.735685e-03, 5.570682e-18, 1.302634e-02, 2.223861e-02, 1.892732e-02, -7.897033e-18, -2.760092e-02, -4.776326e-02,
	-4.211181e-02, 9.388725e-18, 7.379830e-02, 1.585013e-01, 2.250791e-01, 2.489732e-01, 2.213949e-01, 1.533295e-01, 7.018636e-02,
	8.773969e-18, -3.864129e-02, -4.298945e-02, -2.433512e-02, -6.808611e-18, 1.592109e-02, 1.819523e-02, 1.032399e-02, 4.252249e-18,
	-6.369444e-03, -6.823323e-03, -3.509318e-03, -1.656834e-18, 1.447026e-03, 9.269079e-04, 4.617879e-19, -4.621620e-19};
// maximum waiting time when trying to connect
//const float max_waiting_time = 1.0;
// size of the initial probing buffer
//NEED TO UNDERSTAND THE IMPORTANCE OF IT
const float max_waiting_time = 1.0;
// size of the initial probing buffer
//Original size
const int buffer_bytes = 2*1024*1024;
//const int buffer_bytes = 2*1024;
// preferred size of final buffer (note: must be a multiple of 512; also, ca. 32MB is a good size according to BioSemi)
//const int buffer_samples = 100*512;
const int buffer_samples = 10*512;
//original version
//const int buffer_samples = 60*512;
//NEED TO UNDERSTAND THE IMPORTANCE OF IT
//const int buffer_samples = 100*512;
// 64 zero bytes
const unsigned char msg_enable[64] = {0};
// 0xFF followed by 63 zero bytes
const unsigned char msg_handshake[64] = {255,0};
#include <dlfcn.h>
#define LOAD_LIBRARY(lpath) dlopen(lpath,RTLD_NOW | RTLD_LOCAL)
//#define LOAD_LIBRARY(lpath) dlopen(lpath,RTLD_LAZY)
#define LOAD_FUNCTION(lhandle,fname) dlsym(lhandle,fname)
#define FREE_LIBRARY(lhandle) dlclose(lhandle)
#include <dlfcn.h>

const char *dllpath_temp = "/Users/centerformindfulness/.biosemi/liblabview_dll.0.0.1.dylib";
biosemi_io::biosemi_io() {
	uint32_t start_idx; // index of the first sample that was recorded
	uint32_t cur_idx;   // index past the current sample
	dllpath="/Users/centerformindfulness/.biosemi/liblabview_dll.0.0.1.dylib";
	//    biosemi_
	// === load the library & obtain DLL functions ===
	std::cout << "Loading BioSemi driver dll..." << dllpath << std::endl;

	hDLL_ = LOAD_LIBRARY(dllpath_temp);
	std::cout << "hDLL_ "<< hDLL_ << std::endl;
	if (!hDLL_)
		throw std::runtime_error("Could not load BioSemi driver DLL.");
	std::cout << "driver successfully loaded " << dllpath << std::endl;
	OPEN_DRIVER_ASYNC = (OPEN_DRIVER_ASYNC_t)LOAD_FUNCTION(hDLL_,"OPEN_DRIVER_ASYNC");
	if (!OPEN_DRIVER_ASYNC) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Did not find function OPEN_DRIVER_ASYNC() in the BisoSemi driver DLL.");
	}
	USB_WRITE = (USB_WRITE_t)LOAD_FUNCTION(hDLL_,"USB_WRITE");
	if (!USB_WRITE) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Did not find function USB_WRITE() in the BisoSemi driver DLL.");
	}
	READ_MULTIPLE_SWEEPS = (READ_MULTIPLE_SWEEPS_t)LOAD_FUNCTION(hDLL_,"READ_MULTIPLE_SWEEPS");
	if (!READ_MULTIPLE_SWEEPS) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Did not find function READ_MULTIPLE_SWEEPS() in the BisoSemi driver DLL.");
	}
	READ_POINTER = (READ_POINTER_t)LOAD_FUNCTION(hDLL_,"READ_POINTER");
	if (!READ_POINTER) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Did not find function READ_POINTER() in the BisoSemi driver DLL.");
	}
	CLOSE_DRIVER_ASYNC = (CLOSE_DRIVER_ASYNC_t)LOAD_FUNCTION(hDLL_,"CLOSE_DRIVER_ASYNC");
	if (!CLOSE_DRIVER_ASYNC) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Did not find function CLOSE_DRIVER_ASYNC() in the BisoSemi driver DLL.");
	}
	// === initialize driver ===
	// connect to driver
	std::cout << "Connecting to driver..." << std::endl;
	hConn_ = OPEN_DRIVER_ASYNC();
	if (!hConn_) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Could not open connection to BioSemi driver.");
	}
	// initialize USB2 interface
	std::cout << "Initializing USB interface..." << std::endl;
	if (!USB_WRITE(hConn_,&msg_enable[0])) {
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Could not initialize BioSemi USB2 interface.");
	}
	// === allocate ring buffer and begin acquisiton ===
	// initialize the initial (probing) ring buffer
	std::cout << "Initializing ring buffer..." << std::endl;
	ring_buffer_.reset(new uint32_t[buffer_bytes]);
	if (!ring_buffer_) {
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Could not allocate ring buffer (out of memory).");
	}
	std::memset(ring_buffer_.get(),0,buffer_bytes);
	// begin acquisition
	READ_MULTIPLE_SWEEPS(hConn_,(char*)ring_buffer_.get(), buffer_bytes);
	// enable handshake
	std::cout << "Enabling handshake..." << std::endl;
	if (!USB_WRITE(hConn_,&msg_handshake[0])) {
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		ring_buffer_.reset();
		throw std::runtime_error("Could not enable handshake with BioSemi USB2 interface.");
	}
	// === read the first sample ===
	// obtain buffer head pointer
	std::cout << "Querying buffer pointer..." << std::endl;
	if (!READ_POINTER(hConn_,&start_idx)) {
		USB_WRITE(hConn_, &msg_enable[0]);
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		ring_buffer_.reset();
		throw std::runtime_error("Can not obtain ring buffer pointer from BioSemi driver.");
	}
	// check head pointer validity
	if (start_idx > buffer_bytes) {
		USB_WRITE(hConn_, &msg_enable[0]);
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		ring_buffer_.reset();
		throw std::runtime_error("Buffer pointer returned by BioSemi driver is not in the valid range.");
	}
	// read the first sample
	std::cout << "Waiting for data..." << std::endl;
	clock_t start_time = clock();
	while (1) {
		// error checks...
		if (!READ_POINTER(hConn_, &cur_idx)) {
			USB_WRITE(hConn_, &msg_enable[0]);
			CLOSE_DRIVER_ASYNC(hConn_);
			FREE_LIBRARY(hDLL_);
			ring_buffer_.reset();
			throw std::runtime_error("Ring buffer handshake with BioSemi driver failed unexpectedly.");
		}
		if ( ((double)(clock() - start_time))/CLOCKS_PER_SEC > max_waiting_time) {
			USB_WRITE(hConn_, &msg_enable[0]);
			CLOSE_DRIVER_ASYNC(hConn_);
			FREE_LIBRARY(hDLL_);
			ring_buffer_.reset();
			if (cur_idx - start_idx < 8)
				throw std::runtime_error("BioSemi driver does not transmit data. Is the box turned on?");
			else
				throw std::runtime_error("Did not get a sync signal from BioSemi driver. Is the battery charged?");
		}
		if ((cur_idx - start_idx >= 8) && (ring_buffer_.get()[0] == 0xFFFFFF00)) {
			// got a sync signal on the first index...
			start_idx = 0;
			break;
		}
		if ((cur_idx - start_idx >= 8) && (ring_buffer_.get()[start_idx/4] == 0xFFFFFF00))
			// got the sync signal!
			break;
	}
	// === parse amplifier configuration ===
	// read the trigger channel data ...
	std::cout << "Checking status..." << std::endl;
	uint32_t status = ring_buffer_.get()[start_idx/4+1] >> 8;
	std::cout << "  status: " << status << std::endl;
	// determine channel configuration
	is_mk2_ = ((status&(1<<23)) != 0);
	//   is_mk2_  = false;
	std::cout << "  MK2: " << is_mk2_ << std::endl;
	// check speed mode
	speed_mode_ = ((status&(1<<17))>>17) + ((status&(1<<18))>>17) + ((status&(1<<19))>>17) + ((status&(1<<21))>>18);
	std::cout << "  speed mode: " << speed_mode_ << std::endl;
	// check for problems...
	if (speed_mode_ > 9) {
		USB_WRITE(hConn_, &msg_enable[0]);
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		ring_buffer_.reset();
		if (is_mk2_)
			throw std::runtime_error("BioSemi amplifier speed mode wheel must be between positions 0 and 8 (9 is a reserved value); recommended for typical use is 4.");
		else
			throw std::runtime_error("BioSemi amplifier speed mode wheel must be between positions 0 and 8 (9 is a reserved value); recommended for typical use is 4.");
	}
	// determine sampling rate (http://www.biosemi.com/faq/adjust_samplerate.htm)
	switch (speed_mode_ & 3) {
		case 0: srate_ = 2048; break;
		case 1: srate_ = 4096; break;
		case 2: srate_ = 8192; break;
		case 3: srate_ = 16384; break;
	}
	// speed modes lower than 3 are special on Mk2 and are for daisy-chained setups (@2KHz)
	bool multibox = false;
	if (is_mk2_ && speed_mode_ < 4) {
		srate_ = 2048;
		multibox = true;
	}
	std::cout << "  sampling rate: " << srate_ << std::endl;
	// determine channel configuration -- this is written according to:
	//   http://www.biosemi.com/faq/make_own_acquisition_software.htm
	//   http://www.biosemi.com/faq/adjust_samplerate.htm
	if (is_mk2_) {
		// in an Mk2 the speed modes 0-3 are for up to 4 daisy-chained boxes; these are
		// multiplexed, have 128ch EEG each and 8ch EXG each, plus 16 extra channels each (howdy!)
		switch (speed_mode_) {
			case 0:
			case 1:
			case 2:
			case 3:
				nbeeg_ = 4*128; nbexg_ = 4*8; nbaux_ = 4*16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 610; break;
				// spd modes 4-7 are the regular ones and have 8 EXG's added in
			case 4: //nbeeg_ = 128; nbexg_ = 0; nbaux_ = 0; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 129; break;
				nbeeg_ = 256; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 282; break;
			case 5: nbeeg_ = 128; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 154; break;
			case 6: nbeeg_ = 64; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 90; break;
			case 7: nbeeg_ = 32; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 58; break;
				// spd mode 8 adds
			case 8: nbeeg_ = 256; nbexg_ = 8; nbaux_ = 16; nbaib_ = 32; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 314; break;
		}
	} else {
		// in a Mk1 the EXG's are off in spd mode 0-3 and on in spd mode 4-7 (but subtracted from the EEG channels)
		switch (speed_mode_) {
			// these are all-EEG modes
			case 0: nbeeg_ = 256; nbexg_ = 0; nbaux_ = 0; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 258; break;
			case 1: nbeeg_ = 128; nbexg_ = 0; nbaux_ = 0; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 130; break;
			case 2: nbeeg_ = 64; nbexg_ = 0; nbaux_ = 0; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 66; break;
			case 3: nbeeg_ = 32; nbexg_ = 0; nbaux_ = 0; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 34; break;
				// in these modes there are are 8 EXGs and 16 aux channels
			case 4: nbeeg_ = 232; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 258; break;
			case 5: nbeeg_ = 104; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 130; break;
			case 6: nbeeg_ = 40; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 66; break;
			case 7: nbeeg_ = 8; nbexg_ = 8; nbaux_ = 16; nbaib_ = 0; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 34; break;
				// in spd mode 8 there are 32 AIB channels from an Analog Input Box (AIB) multiplexed in (EXGs are off)
			case 8: nbeeg_ = 256; nbexg_ = 0; nbaux_ = 0; nbaib_ = 32; nbtrig_ = 1; nbsync_ = 1; nbchan_ = 290; break;
		}
	}
	//  std::cout << "srate_ " << srate_ << "  channels: " << nbchan_ << "(" << nbsync_ << "xSync, " << nbtrig_ << "xTrigger, " << nbeeg_ << "xEEG, " << nbexg_ << "xExG, " << nbaux_ << "xAUX, " << nbaib_ << "xAIB)" << std::endl;
	// check for additional problematic conditions
	battery_low_ = (status & (1<<22)) != 0;
	if (battery_low_)
		std::cout << "  battery: low" << std::endl;
	else
		std::cout << "  battery: good" << std::endl;
	if (battery_low_)
		std::cout << "The BioSemi battery is low; amplifier will shut down within 30-60 minutes." << std::endl;
	// compute a proper buffer size (needs to be a multiple of 512, a multiple of nbchan, as well as ~32MB in size)
	// std::cout << "Reallocating optimized ring buffer..." << std::endl;
	// === shutdown current coonnection ===
	// shutdown current connection
	std::cout << "Sending the enable message again..." << std::endl;
	if (!USB_WRITE(hConn_, &msg_enable[0])) {
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Error while disabling the handshake.");
	}
	std::cout << "Closing the driver..." << std::endl;
	if (!CLOSE_DRIVER_ASYNC(hConn_)) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Error while disconnecting.");
	}
	// reset to a new ring buffer
	std::cout << "Freeing the ring buffer..." << std::endl;
	ring_buffer_.reset();
	// === reinitialize acquisition ===
	std::cout << "Allocating a new ring buffer..." << std::endl;
	ring_buffer_.reset(new uint32_t[buffer_samples*nbchan_]);
	if (!ring_buffer_) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Could not reallocate ring buffer (out of memory?).");
	}
	std::cout << "Zeroing the ring buffer..." << std::endl;
	std::memset(ring_buffer_.get(),0,buffer_samples*4*nbchan_);
	// reconnect to driver
	std::cout << "Opening the driver..." << std::endl;
	hConn_ = OPEN_DRIVER_ASYNC();
	//std::cout << "hConn_=" << hConn_ << std::endl;
	if (!hConn_) {
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Could not open connection to BioSemi driver.");
	}
	// std::cout << "hConn_=" << hConn_ << std::endl;
	// reinitialize USB2 interface
	std::cout << "Reinitializing the USB interface..." << std::endl;
	if (!USB_WRITE(hConn_,&msg_enable[0])) {
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		throw std::runtime_error("Could not initialize BioSemi USB2 interface.");
	}
	std::cout << "hConn_=" << hConn_ << std::endl;
	// begin acquisition
	std::cout << "Starting data acquisition..." << std::endl;
	READ_MULTIPLE_SWEEPS(hConn_,(char*)ring_buffer_.get(),buffer_samples*4*nbchan_);
	// enable handshake
	std::cout << "Enabling handshake..." << std::endl;
	if (!USB_WRITE(hConn_,&msg_handshake[0])) {
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		ring_buffer_.reset();
		throw std::runtime_error("Could not reenable handshake with BioSemi USB2 interface.");
	}
	// === check for correctness of new data ===
	// make sure that we can read the buffer pointer
	std::cout << "Attempt to read buffer pointer..." << std::endl;
	if (!READ_POINTER(hConn_,&start_idx)) {
		USB_WRITE(hConn_, &msg_enable[0]);
		CLOSE_DRIVER_ASYNC(hConn_);
		FREE_LIBRARY(hDLL_);
		ring_buffer_.reset();
		throw std::runtime_error("Can not obtain ring buffer pointer from BioSemi driver.");
	}
	std::cout << "Verifying channel format..." << std::endl;
	start_time = clock();
	while (1) {
		// error checks
		if (!READ_POINTER(hConn_, &cur_idx)) {
			USB_WRITE(hConn_, &msg_enable[0]);
			CLOSE_DRIVER_ASYNC(hConn_);
			FREE_LIBRARY(hDLL_);
			ring_buffer_.reset();
			throw std::runtime_error("Ring buffer handshake with BioSemi driver failed unexpectedly.");
		}
		if ( ((double)(clock() - start_time))/CLOCKS_PER_SEC > max_waiting_time) {
			USB_WRITE(hConn_, &msg_enable[0]);
			CLOSE_DRIVER_ASYNC(hConn_);
			FREE_LIBRARY(hDLL_);
			ring_buffer_.reset();
			if (cur_idx - start_idx < 8)
				throw std::runtime_error("BioSemi driver does not transmit data. Is the box turned on?");
			else
				throw std::runtime_error("Did not get a sync signal from BioSemi driver. Is the battery charged?");
		}
		if ((cur_idx - start_idx >= 4*nbchan_) && (ring_buffer_.get()[0] == 0xFFFFFF00))
			// got a sync signal on the first index
			start_idx = 0;
		if ((cur_idx - start_idx >= 4*nbchan_) && (ring_buffer_.get()[start_idx/4] == 0xFFFFFF00)) {
			if (ring_buffer_.get()[start_idx/4+nbchan_] != 0xFFFFFF00) {
				USB_WRITE(hConn_, &msg_enable[0]);
				CLOSE_DRIVER_ASYNC(hConn_);
				FREE_LIBRARY(hDLL_);
				ring_buffer_.reset();
				throw std::runtime_error("Sync signal did not show up at the expected position.");
			} else {
				std::cout << "Channel format is correct..." << std::endl;
				// all correct
				break;
			}
		}
	}
	// === derive channel labels ===
	channel_types_.clear();
	// regular setup
	for (int k = 1; k <= 128; k++) {
		std::string tmp = "A"; tmp[0] = 'A'+(k-1)/32;
		channel_labels_.push_back(std::string(tmp) += std::to_string(1+(k-1)%32));
		channel_types_.push_back("EEG");
	}
	for (int k=1;k<=5;k++) {
		channel_labels_.push_back(std::string("EX") += std::to_string(k));
		channel_types_.push_back("EXG");
	}
	channel_labels_.push_back(std::string("AUX") += std::to_string(1));
	channel_types_.push_back("AUX");
	std::cout << channel_labels_.size() << " channel labels " << std::endl;
	for(int i =0; i < channel_labels_.size();i++)
		std::cout << channel_labels_[i] <<  " ";
	std::cout << std::endl;
	std::cout << "hConn_=" << hConn_ << std::endl;
	last_idx_ = 0;
	int channels = channel_labels_.size();
	weight_vector.resize(channels);
	for(int i = 0; i < channels; i++)
		weight_vector[i] = 1.0;
	//    std::memset(&weight_vector[0], 1.0, 128*sizeof(float));
	std::string weight_vector_filename("/Users/centerformindfulness/.biosemi/biosemi_weight_vector.txt");
	ifstream weight_vector_file( weight_vector_filename.c_str());
	if(weight_vector_file.good()){
		cout << "found weight file " << weight_vector_filename << endl;
		for (int i = 0; i < channels; i++) {
			float temp_weight;
			weight_vector_file >> temp_weight;
			weight_vector[i] = temp_weight;
			if(weight_vector_file.eof()){
				break;
				cout << "weight file is not properly formatted" << endl;
				// throw "improper weight file ";
			}
		}
	}else {
		cout << "weight file not found...running with default weight 1 on each channels" << endl;
	}
	cout << "wieght vector: " ;
	copy(weight_vector.begin(),weight_vector.end(),std::ostream_iterator<float>(cout," "));
	cout << endl;
}
biosemi_io::~biosemi_io() {
	std::cout << "biosemi_io destroyed" << std::endl;
	/* close driver connection */
	try{
		std::cout << "handshake during shutting down " << std::endl;
		if (!USB_WRITE(hConn_,&msg_enable[0]))
			std::cout << "The handshake while shutting down the BioSemi driver gave an error." << std::endl;
		else {        std::cout << "handshake during shutting down successful" << std::endl;}
		if (!CLOSE_DRIVER_ASYNC(hConn_))
			std::cout << "Closing the BioSemi driver gave an error." << std::endl;
		// close the DLL
		FREE_LIBRARY(hDLL_);
	}
	catch(std::exception e){ cout << "exception caught "  << e.what() << " during ring buffer free" << endl;}
	catch(...){ cout << "other exception during ring buffer free" << endl;}
	std::cout << "ring buffer freed" << std::endl;
	// free the ring buffer
	ring_buffer_.reset();
}
void biosemi_io::get_chunk(chunk_t &result) {
	std::cout <<  " entering function void biosemi_io::get_chunk(chunk_t &result) " << this << std::endl;
	//get current buffer offset
	int cur_idx;
	if (!READ_POINTER(hConn_, (unsigned*)&cur_idx))
		throw std::runtime_error("Reading the updated buffer pointer gave an error.");
	cur_idx = cur_idx/4;
	// forget about incomplete sample data
	cur_idx = cur_idx - cur_idx % nbchan_;
	if (cur_idx < 0)
		cur_idx = cur_idx + buffer_samples*nbchan_;
	std::cout <<  " result.size before result.clear() " << result.size() << std::endl;
	result.clear();
	if (cur_idx != last_idx_) {
		if (cur_idx > last_idx_) {
			// sequential read: copy intermediate part between offsets
			int chunklen = (cur_idx - last_idx_) / nbchan_;
			std::cout << "Inside cur_idx > last_idx_"<< " chunklen = " << chunklen << std::endl;
			result.resize(chunklen);
			for (int k=0;k<chunklen;k++) {
				result[k].resize(nbchan_);
				memcpy(&result[k][0], &ring_buffer_.get()[last_idx_ + k*nbchan_], nbchan_*4);
			}
		}
		else {
			// wrap-around read: concatenate two parts
			int chunklen = (cur_idx + buffer_samples*nbchan_ - last_idx_) / nbchan_;
			std::cout << "Inside cur_idx <= last_idx_"<< " chunklen = " << chunklen << std::endl;
			result.resize(chunklen);
			int first_section = buffer_samples - last_idx_/nbchan_;
			for (int k=0;k<first_section;k++) {
				result[k].resize(nbchan_);
				memcpy(&result[k][0], &ring_buffer_.get()[last_idx_ + k*nbchan_], nbchan_*4);
			}
			int second_section = chunklen - first_section;
			for (int k=0;k<second_section;k++) {
				result[first_section+k].resize(nbchan_);
				memcpy(&result[first_section+k][0], &ring_buffer_.get()[k*nbchan_], nbchan_*4);
			}
		}
		// update status flags
		uint32_t status = ring_buffer_.get()[cur_idx] >> 8;
		battery_low_ = (status & (1<<22)) != 0;
	}
	// update last buffer pointer
	last_idx_ = cur_idx;
	//   std::cout <<  " result.size at the end of get_chunk  " << result.size() << std::endl;
}
void biosemi_io::get_chunk() {
	static time_t start = 0;
	static int k = 0;
	static int second_section = 0;
	static double total_samples  = 0;
	//std::cout <<  " entering function void biosemi_io::get_chunk(chunk_t &raw_chunk_) " << this << std::endl;
	//get current buffer offset
	static int cur_idx;
	static int chunklen;
	try{
		if (!READ_POINTER(hConn_, (unsigned*)&cur_idx))
			throw std::runtime_error("Reading the updated buffer pointer gave an error.");
		//cout << cur_idx << endl;
		//Why are we doing this ?
		cur_idx = cur_idx/4;
		// forget about incomplete sample data
		cur_idx = cur_idx - cur_idx % nbchan_;
		if (cur_idx < 0)
			cur_idx = cur_idx + buffer_samples*nbchan_;
		// std::cout <<  " raw_chunk_.size before raw_chunk_.clear() " << raw_chunk_.size() << std::endl;
		raw_chunk_.clear();
		if (cur_idx != last_idx_) {
			if (cur_idx > last_idx_) {
				//     cout << "nr" << endl;
				// sequential read: copy intermediate part between offsets
				chunklen = (cur_idx - last_idx_) / nbchan_;
				//std::cout << "Inside cur_idx > last_idx_"<< " chunklen = " << chunklen << std::endl;
				raw_chunk_.resize(chunklen);
				for ( k = 0; k < chunklen; k++) {
					raw_chunk_[k].resize(nbchan_);
					memcpy(&raw_chunk_[k][0], &ring_buffer_.get()[last_idx_ + k*nbchan_], nbchan_* 4);
				}
			} else {
				// cout << "war" << endl;
				// wrap-around read: concatenate two parts
				chunklen = (cur_idx + buffer_samples*nbchan_ - last_idx_) / nbchan_;
				//std::cout << "Inside cur_idx <= last_idx_"<< " chunklen = " << chunklen << std::endl;
				raw_chunk_.resize(chunklen);
				int first_section = buffer_samples - last_idx_/nbchan_;
				for ( k = 0; k < first_section; k++) {
					raw_chunk_[k].resize(nbchan_);
					memcpy(&raw_chunk_[k][0], &ring_buffer_.get()[last_idx_ + k*nbchan_], nbchan_*4);
				}
				second_section = chunklen - first_section;
				for ( k = 0; k < second_section; k++) {
					raw_chunk_[first_section+k].resize(nbchan_);
					memcpy(&raw_chunk_[first_section+k][0], &ring_buffer_.get()[k*nbchan_], nbchan_*4);
				}
			}//wrap around read
			// update status flags
			uint32_t status = ring_buffer_.get()[cur_idx] >> 8;
			battery_low_ = (status & (1<<22)) != 0;
			// update last buffer pointer
			last_idx_ = cur_idx;
			//test_file.open();
			chunk_transform<int32_t, float>(raw_chunk_,resampled_chunk_, weight_vector);
			//    cout << "resampled_chunk_.size(): " << resampled_chunk_.size() << endl;
			for(int i = 0; i < resampled_chunk_.size();i++)
				outlet->push_sample(resampled_chunk_[i]);
		}
	}//try ends
	catch(std::exception e){
		cout << "exception " << e.what() << endl;
		exit(0);
	}
	catch(...){
		cout << "exception of other types" << endl;
		exit(0);
	}
}
void biosemi_io::get_raw_chunk_pointer(chunk_t& raw_chunk_pointer) const{
	raw_chunk_pointer = raw_chunk_;
}
void biosemi_io::get_resampled_chunk_pointer(chunk_t& resampled_chunk_pointer) const{
	resampled_chunk_pointer = raw_chunk_;
}
void biosemi_io::printChunk(std::vector< std::vector<double> >& chunk_data) const{
	for(size_t i = 0; i < chunk_data.size(); i++){
		cout << endl;
		for(size_t j = 0; j < chunk_data[i].size()-1;j++){
			cout << chunk_data[i][j] <<",";
			cout << chunk_data[i][chunk_data[i].size()-1] <<" ";
		}
	}
}
void biosemi_io::printChunk(chunk_t& chunk_data) const{
	for(size_t i = 0; i < chunk_data.size(); i++){
		cout << endl;
		for(size_t j = 0; j < chunk_data[i].size()-1;j++){
			cout << chunk_data[i][j] <<",";
			cout << chunk_data[i][chunk_data[i].size()-1] <<" ";
		}
	}
}
