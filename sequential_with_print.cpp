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


void resize_image(std::string image_name,std::string dest,CImg<unsigned char> watermark)
{
    CImg<unsigned char> image_to_resize(image_name.c_str());
    image_to_resize.resize(watermark.width(),watermark.height(),1,3);
    std::string new_image_name = image_name.substr(image_name.find_last_of("/"));
    image_to_resize.save_jpeg((dest + new_image_name).c_str());
    std::cout<<"Resized "<<new_image_name<<std::endl;

}

int main(int argc, char* argv[])
{
    if (argc < 4 || argc > 5){
        std::cout<<"Il numero di argomenti è sbagliato"<<std::endl;
    }

    std::queue<std::string>images_names;    //Queue che contiene il path delle immagini di input
    std::string watermark_name = argv[3];   //Il nome dell'immagine di watermark
    CImg<unsigned char> watermark(watermark_name.c_str());  //L'immagine di watermark
    int images_processed = 0;

    // OPZIONALE: Nel caso argv[5] == 1 ridimensiona tutte le immagini della cartella
    // di input nella dimensione 500x500
    // Inserisce il path di tutte le immagini della cartella di input nella queue

    if (argv[4] != NULL && atoi(argv[4]) == 1) {
    std::cout << "Resize of the images..."<< std::endl;
        for(auto& p: fs::directory_iterator(argv[1])){
            resize_image(p.path().string(),argv[1],watermark);
        }

    }


    // - Per ogni immagine nella cartella
    // - Estrai il path dell'immagine e crea l'immagine
    // - Per ogni pixel del watermark considera il valore
    // - Essendo il watermark bianco e nero se il valore del pixell è diverso da 255
    //   significa che siamo in corrispondenza del logo e quindi va modificata l'immagine
    //   di input.
    // - L'immagine viene modificata calcolando la media tra il colore originale mappato
    //   in scala di grigio e il colore nero
    // - Si assegna il valore così calcolato alle 3 componenti R-G-B dell'immagine
    // - L'immagine viene poi salvata su disco
    auto total_msec_read = 0;
    auto total_msec_wat = 0;
    auto total_msec_write = 0;
    auto start   = std::chrono::high_resolution_clock::now();
    CImg<unsigned char> image;
    std::string current_image_name;
    for(auto& p: fs::directory_iterator(argv[1])){
        current_image_name= (p.path().string());
        auto start_read   = std::chrono::high_resolution_clock::now();
        image.assign(current_image_name.c_str());
        auto elapsed_read = std::chrono::high_resolution_clock::now() - start_read;
        auto msec_read    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_read).count();
        auto start_watermark   = std::chrono::high_resolution_clock::now();
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
        auto elapsed_watermark = std::chrono::high_resolution_clock::now() - start_watermark;
        auto msec_watermark    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_watermark).count();
        std::string save_image_name = current_image_name.substr(current_image_name.find_last_of("/"));
        auto start_save   = std::chrono::high_resolution_clock::now();
        image.save_jpeg((argv[2] + save_image_name).c_str());
        auto elapsed_save = std::chrono::high_resolution_clock::now() - start_save;
        auto msec_save    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_save).count();
        images_processed += 1;
        image.clear();
        std::cout << "Read      time      : " << msec_read << " msecs " << std::endl;
        std::cout << "Watermark time      : " << msec_watermark << " msecs " << std::endl;
        std::cout << "write     time      : " << msec_save << " msecs " << std::endl;
        total_msec_read += msec_read;
        total_msec_wat += msec_watermark;
        total_msec_write += msec_save;
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "Elapsed time      : " << msec << " msecs " << std::endl;
    std::cout << "Total read : " << total_msec_read / 812 <<std::endl;
    std::cout << "Total water : " << total_msec_wat / 812 <<std::endl;
    std::cout << "Total write : " << total_msec_write / 812 <<std::endl;
    return 0;
}

