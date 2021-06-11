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
struct Task {

	Task(std::string &name):name(name) {};
	std::string         name;
};


// Emitter
struct Reader: ff_node_t<Task> 
{
	Reader(std::string &input_directory):  input_directory(input_directory){}
	Task *svc(Task *task)
	{	
		 CImg<unsigned char> image;
		 for(auto& p: fs::directory_iterator(input_directory)){
             std::string image_name = p.path().string();
			  Task *t = new Task(image_name);
			  ff_send_out(t);
        }
	   return EOS;
	}

	std::string input_directory;
};

// Read + watermark + save
struct Watermarker: ff_node_t<Task>
{
	Watermarker(std::string &watermark_name, std::string output){
		watermark.assign(watermark_name.c_str());
		output_directory = output;
	}

	Task *svc(Task *input)
	{
	
		CImg<unsigned char> image (input->name.c_str());
		std::string image_name = input->name;
		
		cimg_forXY(watermark, x, y) {
                int value = (int)watermark(x, y, 0, 0);

                if (value != 255) {
                    int gray_scale = (image(x, y, 0, 0) + image(x, y, 0, 1) + image(x, y, 0, 2)) / 3;
                    int average = (gray_scale + 255) / 2;
                    image(x, y, 0, 0) = average;
                    image(x, y, 0, 1) = average;
                    image(x, y, 0, 2) = average;
                }
	
            }
         
		std::string save_image_name = image_name.substr(image_name.find_last_of("/"));
		try{
             image.save_jpeg((output_directory + save_image_name).c_str());
        }catch(CImgIOException ex){
            std::cerr << "Errore di salvataggio " << save_image_name << std::endl;
        }
		images_processed += 1;
        image.assign();
        delete input;
        return GO_ON;
	}

	void svc_end()
	{
		total_images_processed += images_processed;
	}

	CImg<unsigned char> watermark;
	int images_processed = 0;
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
   	   workers.push_back(make_unique<Watermarker>(watermark_name,output_directory));
   }

   Reader reader(input_directory);

   ff_Farm<Task> farm(std::move(workers),reader);
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
