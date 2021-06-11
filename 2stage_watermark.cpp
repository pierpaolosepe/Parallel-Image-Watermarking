#include <iostream>
#include <atomic>
#include "CImg.h"
#include <experimental/filesystem>
#include <string>
#include "util.cpp"
#include <vector>
#include <thread>

#define EOS NULL
using namespace cimg_library;
namespace fs = std::experimental::filesystem::v1;

std::atomic<int> total_images_processed = 0;
using image = CImg<unsigned char>;
queue<std::string> image_path_queue;
image watermark;


void read_and_watermark_image(queue<std::pair<image,std::string>> &img_to_save_queue, int id)
{
   int images_processed = 0;
   std::pair<image,std::string> image_and_name;
   while(true)
   {
     std::string image_path = image_path_queue.pop();
     if(image_path == "NULL") {
       total_images_processed += images_processed;
       image_path_queue.push("NULL");
       img_to_save_queue.push(std::pair(image(),"NULL"));	
       return;
	 }

     image img(image_path.c_str());
     cimg_forXY(watermark, x, y) {
         int value = (int)watermark(x, y, 0, 0);
			if (value != 255) {
               int gray_scale = (img(x, y, 0, 0) + img(x, y, 0, 1) + img(x, y, 0, 2)) / 3;
               int average = (gray_scale + 255) / 2;
               img(x, y, 0, 0) = average;
               img(x, y, 0, 1) = average;
               img(x, y, 0, 2) = average;
            }
     }
     std::string image_name = (image_path.substr(image_path.find_last_of("/") + 1));
	 images_processed += 1;
     image_and_name.first = img;
     image_and_name.second = image_name;
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
   std::vector<queue<std::pair<image,std::string>>> img_to_save_queue(num_workers);
   auto start   = std::chrono::high_resolution_clock::now();
   for(auto& p: fs::directory_iterator(input_directory)){
      std::string image_name = p.path().string();
	  image_path_queue.push(image_name);
			
	}
   image_path_queue.push("NULL");
    
    std::thread readers_watermarkers[num_workers];
	std::thread writers[num_workers];
    
    for(int i =  0; i < num_workers; i++)
    {
	   readers_watermarkers[i] = std::thread(read_and_watermark_image,std::ref(img_to_save_queue[i]),i);
	   writers[i]              = std::thread(write_image,std::ref(img_to_save_queue[i]),output_directory,i);	
	}
   
   for(int i = 0; i < num_workers; i++)
   {
	readers_watermarkers[i].join();
    writers[i].join();
   }
   
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "Number of workers : " << num_workers << std::endl; 
    std::cout << "Images processed  : " << total_images_processed << std::endl;
    std::cout << "Elapsed time      : " << msec << " msecs " << std::endl;
   
   return 0;
}
