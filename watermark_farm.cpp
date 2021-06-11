#include <string>
#include <iostream>
#include <experimental/filesystem>
#include <fstream>
#include "CImg.h"
#include <queue>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>


using namespace cimg_library;
namespace fs = std::experimental::filesystem::v1;

std::mutex queue_mutex;

void resize_image(std::string image_name,std::string dest)
{
    CImg<unsigned char> image_to_resize(image_name.c_str());
    image_to_resize.resize(500,500,1,3);
    std::string new_image_name = image_name.substr(image_name.find_last_of("/"));
    image_to_resize.save_jpeg((dest + new_image_name).c_str());
    std::cout<<"Resized "<<new_image_name<<std::endl;

}

    
    // - Per ogni pixell del watermark considera il valore
    // - Essendo il watermark bianco e nero se il valore del pixell è diverso da 255
    // significa che siamo in corrispondenza del logo e quindi va modificata l'immagine
    // di input.
    // - L'immagine viene modificata calcolando la media tra il colore originale mappato
    // in scala di grigio e il colore nero
    // - Si assegna il valore così calcolato alle 3 componenti R-G-B dell'immagine
    // - L'immagine viene poi salvata su disco
void watermarking(std::queue<std::string> &images_names, CImg<unsigned char>& watermark,
		   std::string output_dir, int id, std::atomic<int> &total_images_processed)
{
    
    int images_processed = 0;
    CImg<unsigned char> image;
    
    while (true)
    {
	std::string current_image_name;
        
	queue_mutex.lock();
        if (!images_names.empty()){
        	current_image_name = images_names.front();
		images_names.pop();
	} else{
	   queue_mutex.unlock();
	   total_images_processed += images_processed;
	   return;
	}
	queue_mutex.unlock();

        image.assign(current_image_name.c_str());
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
        std::string save_image_name = current_image_name.substr(current_image_name.find_last_of("/"));
	
        
		try{
            image.save_jpeg((output_dir + save_image_name).c_str());
        }catch(CImgIOException ex){
            std::cerr << "Errore di salvataggio " << save_image_name << std::endl;
        }
        //std::cout<< "Salvata " << save_image_name <<"Thread: " << id <<"Processate: " <<image_processed << std::endl;
	    images_processed += 1;
        image.clear();
        

    }
   
}


int main(int argc, char* argv[])
{
    if (argc < 5 || argc > 6){
        std::cout<<"Il numero di argomenti è sbagliato"<<std::endl;
    }

    std::queue<std::string>images_names;    //Queue che contiene il path delle immagini di input
    std::string watermark_name = argv[3];   //Il nome dell'immagine di watermark
    CImg<unsigned char> watermark(watermark_name.c_str());  //L'immagine di watermark
    int num_workers = atoi(argv[4]);         //Parallelism degree
    std::atomic<int> images_processed = 0;

    // OPZIONALE: Nel caso argv[5] == 1 ridimensiona tutte le immagini della cartella
    // di input nella dimensione 500x500
    // Inserisce il path di tutte le immagini della cartella di input nella queue

    if (argv[5] != NULL && atoi(argv[5]) == 1) {
	std::cout << "Resize of the images..."<< std::endl;
        for(auto& p: fs::directory_iterator(argv[1])){
            images_names.push(p.path().string());
            resize_image(p.path().string(),argv[1]);
        }

    } else {
        for(auto& p: fs::directory_iterator(argv[1])){
            images_names.push(p.path().string());
        }
    }

    std::cout<<"Total images      : "<<images_names.size()<<std::endl;
    auto start   = std::chrono::high_resolution_clock::now();
    std::thread workers[num_workers];
    for (int i = 0; i < num_workers; i++) {
        workers[i] = std::thread(watermarking,std::ref(images_names),std::ref(watermark),(std::string)argv[2], i,std::ref(images_processed));
    }
    for (int i = 0; i < num_workers; i++) {
            workers[i].join();
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "Number of workers : " << num_workers << std::endl; 
    std::cout << "Images processed  : " << images_processed << std::endl;
    std::cout << "Elapsed time      : " << msec << " msecs " << std::endl;

    return 0;
}

