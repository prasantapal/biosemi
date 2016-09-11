#include <iostream>
#include <algorithm>
#include <string>
#include <csignal>
#include <sstream>
#include <fstream>
#include <lsl_cpp.h>

const std::string  __DIRECTORY_SEPARATOR__={"/"};

std::string get_home_dir(){
	std::string temp_path=__DIRECTORY_SEPARATOR__ + "tmp" + __DIRECTORY_SEPARATOR__ + "tmp_tmp_tmp_tmp_3.13159.txt";
	std::string system_call_command="echo $HOME > " + temp_path;
	int sys_call_id=std::system(system_call_command.c_str());
	std::stringstream ss("");
	ss << std::ifstream(temp_path.c_str()).rdbuf();
	std::string unlink_command="unlink " + temp_path;
	sys_call_id=std::system(unlink_command.c_str());
	std::cout << ss.str() << std::endl;
	std::string str=ss.str();
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
	return str;
}
std::string get_config_path() {
	std::string project_name="biosemi";
	std::string home_dir=get_home_dir();
	std::string config_dir="."+ project_name;
	return home_dir + __DIRECTORY_SEPARATOR__ + config_dir;

}


int main(int argc, char *argv[])
{	

	std::string name="test",type="eeg";
	int no_of_channels=10;

	lsl::stream_info info(name,type,no_of_channels,100,lsl::cf_float32,std::string(name)+=type);
	try{
		lsl::stream_outlet outlet(info);    

	}catch(std::exception& e){
		std::cout << "caught exception " << e.what() << std::endl;
	}
	catch(...){
	}
	char c=getchar();
	return 0;
}

