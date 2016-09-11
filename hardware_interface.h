//  hardware_interface.h
//  AcquisitionStandAlone
//  Created by Dr. Prasanta Pal
//
#ifndef HARDWARE_INTERFACE_H
#define HARDWARE_INTERFACE_H
#include <lsl_cpp.h>
#include <thread>
#include <fstream>
#include <string>

#include "biosemi_io.h"
class hardware_interface
{
	public:
		void connect_amp();
		void load_config(const std::string &filename);
		void save_config(const std::string &filename);
		void set_home_dir(const std::string& home_dir){home_dir_=home_dir; }
		std::string get_home_dir() const { return home_dir_;}
	private:
		void read_thread_function();
		// the biosemi backend
		std::shared_ptr<biosemi_io> biosemi_;
		std::shared_ptr<std::thread> reader_thread_;
		// misc meta-data from the config
		std::string location_system_;
		std::string location_manufacturer_;
		std::vector<std::string> channels_;
		std::vector<std::string> types_;
		std::vector<int> index_set_;
		std::vector<std::vector<double> > scaled_chunk_tr_;
		std::vector<std::vector<double> > resampled_chunk_tr_;
		std::vector<std::vector<int> > resampled_chunk_;
		int capCircumference;
		std::vector<std::string> capDesign;
		int capDesign_index;
		std::string capLocation;
		std::string referenceChannels;
		int channelSubset_index;
		bool resamplingOn;
		std::vector<std::string> channelSubset;
		std::string home_dir_;
};
#endif
