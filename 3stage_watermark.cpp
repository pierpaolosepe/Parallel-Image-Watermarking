#include <iostream>
#include <atomic>
#include "CImg.h"
#include <experimental/filesystem>
#include <string>
#include "util.cpp"
#include <vector>
#include <thread>

using namespace cimg_library;
namespace fs = std::experimental::filesystem::v1;

std::atomic<int> total_images_processed = 0;
using image = CImg<unsigned char>;
queue<std::string> image_path_queue;
image watermark;


void read_image(queue<std::pair<image,std::string>> &img_to_proc_queue, int id)
{
   while(true)
   {
     std::string image_path = image_path_queue.pop();
     if(image_path == "NULL") {
       image_path_queue.push("NULL");
       img_to_proc_queue.push(std::pair(image(),"NULL"));
       return;
	 }
     image img(image_path.c_str());
	 std::string image_name = (image_path.substr(image_path.find_last_of("/") + 1));
	 //std::cout<<image_name<<std::endl;
     img_to_proc_queue.push(std::pair(img,image_name));
     
   }
};

void watermark_image(queue<std::pair<image,std::string>> &img_to_proc_queue,
	                 queue<std::pair<image,std::string>> &img_to_save_queue,
	                 int id)
{
   int images_processed = 0;
   while(true)
   {
	  std::pair image_and_name = img_to_proc_queue.pop();
      if (image_and_name.second == "NULL") {
		total_images_processed += images_processed;
        img_to_save_queue.push(image_and_name);
		return;
	  }
      image image_to_process = image_and_name.first;
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
      image_and_name.first = image_to_process;
	  img_to_save_queue.push(image_and_name);
   }
    
}

void write_image(queue<std::pair<image,std::string>> &img_to_save_queue,std::string output_directory,int id)
{
    while(true)
	{
		std::pair image_and_name = img_to_save_queue.pop();
        if(image_and_name.second == "NULL") {
			return;
		}
		image img = image_and_name.first;
	    std::string image_name = image_and_name.second;
        std::string output_name(output_directory + "/" + image_name);
        try{
            img.save_jpeg(output_name.c_str());
        }catch(CImgIOException ex){
            std::cerr << "Errore di salvataggio " << output_name << std::endl;
        }
        
        
	}
}


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
	
   watermark.assign(watermark_name.c_str());
   std::vector<queue<std::pair<image,std::string>>> img_to_proc_queue(num_workers), img_to_save_queue(num_workers);
   auto start   = std::chrono::high_resolution_clock::now();
   for(auto& p: fs::directory_iterator(input_directory)){
      std::string image_name = p.path().string();
	  image_path_queue.push(image_name);
			
	}
   image_path_queue.push("NULL");
    
    std::thread readers[num_workers];
    std::thread watermarkers[num_workers];
	std::thread writers[num_workers];
    
    for(int i =  0; i < num_workers; i++)
    {
	   readers[i]      = std::thread(read_image,std::ref(img_to_proc_queue[i]),i);
	   watermarkers[i] = std::thread(watermark_image,std::ref(img_to_proc_queue[i]),std::ref(img_to_save_queue[i]),i);
	   writers[i]      = std::thread(write_image,std::ref(img_to_save_queue[i]),output_directory,i);	
	}
   
   for(int i = 0; i < num_workers; i++)
   {
	readers[i].join();
    watermarkers[i].join();
    writers[i].join();
   }
   
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "Number of workers : " << num_workers << std::endl; 
    std::cout << "Images processed  : " << total_images_processed << std::endl;
    std::cout << "Elapsed time      : " << msec << " msecs " << std::endl;
   
   return 0;
}
