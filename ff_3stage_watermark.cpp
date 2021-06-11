#include <iostream>
#include <atomic>
#include "CImg.h"
#include <experimental/filesystem>
#include <string>
#include <ff/pipeline.hpp>
#include <ff/farm.hpp>

using namespace ff;
using namespace cimg_library;
namespace fs = std::experimental::filesystem::v1;

std::atomic<int> total_images_processed = 0;
using image = CImg<unsigned char>;


struct Emitter: ff_node_t<std::string> 
{
	Emitter(std::string &input_directory):  input_directory(input_directory){}

	std::string *svc(std::string *input)
	{	

		 for(auto& p: fs::directory_iterator(input_directory)){
              std::string *image_name = new std::string(p.path().string());
			  ff_send_out(image_name);
        }
	   return EOS;
	}

	std::string input_directory;
};

struct Reader: ff_node_t<std::string, std::pair<image*, std::string>>
{
	std::pair<image*, std::string>	*svc( std::string *image_path)
	{
		image *img = new image(image_path->c_str());
	    std::string image_name = (image_path->substr(image_path->find_last_of("/") + 1));
	    std::pair<image *,std::string> *image_and_name = new std::pair<image *,std::string>(img,image_name);
		delete image_path;
		return image_and_name;
	}


};

// Second Stage
struct Watermarker: ff_node_t<std::pair<image*, std::string>>
{
	Watermarker(std::string &watermark_name){
		watermark.assign(watermark_name.c_str());
	}

	std::pair<image*, std::string> *svc(std::pair<image*, std::string> *input)
	{
		image &image_to_process = *(input->first);		
		cimg_forXY(watermark, x, y) {
                int value = (int)watermark(x, y, 0, 0);

                if (value != 255) {
                    int gray_scale = (image_to_process(x, y, 0, 0) + image_to_process(x, y, 0, 1) + image_to_process(x, y, 0, 2)) / 3;
                    int average = (gray_scale + 255) / 2;
                    image_to_process(x, y, 0, 0) = average;
                    image_to_process(x, y, 0, 1) = average;
                    image_to_process(x, y, 0, 2) = average;
                }
	
            }
		images_processed += 1;
        return input;
		
	}

	void svc_end()
	{
		total_images_processed += images_processed;
	}

	CImg<unsigned char> watermark;
	int images_processed = 0;
   
};

//Third Stage
struct Writer: ff_node_t <std::pair<image*, std::string>>
{
	Writer(std::string &output_directory):  output_directory(output_directory){}

	std::pair<image*, std::string> *svc(std::pair<image*, std::string> *input)
	{
        std::string output_name(output_directory + "/" + input->second);
		try{
             input->first->save_jpeg(output_name.c_str());
        }catch(CImgIOException ex){
            std::cerr << "Errore di salvataggio " << output_name << std::endl;
        }
        delete input->first;
        delete input;
        return GO_ON;
	
	}

	std::string output_directory;
};

int main (int argc, char *argv[])
{
   if(argc != 5) {
   	 std::cerr<<"Errore parametri in input"<<std::endl;
   	 return -1;
   }
   std::string input_directory  = argv[1];
   std::string output_directory = argv[2];
   std::string watermark_name   = argv[3];
   int num_workers              = atoi(argv[4]);

   std::vector<std::unique_ptr<ff_node>> workers;
   for(int i = 0; i < num_workers; i++)
   {
	   Reader* 		reader 		= new Reader();
       Watermarker* watermarker = new Watermarker(watermark_name);
       Writer* 		writer 		= new Writer(output_directory);	
   	   workers.push_back(make_unique<ff_Pipe<>>(reader,watermarker,writer));
   }

   Emitter emitter(input_directory);
   ff_Farm<> farm(std::move(workers),emitter);
   farm.set_scheduling_ondemand();
   farm.remove_collector();
   auto start   = std::chrono::high_resolution_clock::now();	
   if(farm.run_and_wait_end() < 0)
   {
   	  error("running farm\n");
   	  return -1;
   }
   
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "Number of workers : " << num_workers << std::endl; 
    std::cout << "Images processed  : " << total_images_processed << std::endl;
    std::cout << "Elapsed time      : " << msec << " msecs " << std::endl;
   
   return 0;
}
