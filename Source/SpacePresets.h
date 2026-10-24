#pragma once
#include <array>
// Original algorithmic profiles. Names identify design references only.
// No measured room models, EastWest IRs, photos or "True Stereo" claims.
namespace ir {
struct SpacePreset {
    const char* name; const char* category;
    int room;
    float predelay,decay,width,damping,lowCut,highCut,amount;
};
inline constexpr std::array<SpacePreset,55> spacePresets{{
    {"Abandoned Abbey","Churches",2,52.0f,0.88f,0.95f,0.69f,170.0f,6200.0f,0.4f},
    {"Angels Cathedral","Churches",2,39.0f,0.76f,1.0f,0.49f,110.0f,9100.0f,0.37f},
    {"Berlin Church","Churches",2,28.0f,0.64f,0.89f,0.57f,130.0f,7800.0f,0.33f},
    {"Grace Church","Churches",2,32.0f,0.58f,0.91f,0.54f,145.0f,8300.0f,0.31f},
    {"Hamburg Cathedral","Churches",2,61.0f,0.91f,1.0f,0.62f,125.0f,7100.0f,0.41f},
    {"LA Cathedral","Churches",2,47.0f,0.82f,0.98f,0.48f,105.0f,9700.0f,0.38f},
    {"LA Church","Churches",2,24.0f,0.49f,0.84f,0.6f,150.0f,7600.0f,0.29f},
    {"NY Church","Churches",2,35.0f,0.67f,0.87f,0.65f,160.0f,6900.0f,0.34f},
    {"Sacred Heart","Churches",2,43.0f,0.73f,0.94f,0.52f,135.0f,8600.0f,0.35f},
    {"Santa Barbara Temple","Churches",3,22.0f,0.61f,0.86f,0.45f,120.0f,10200.0f,0.3f},
    {"St. Dominic","Churches",2,54.0f,0.85f,0.97f,0.58f,115.0f,8000.0f,0.39f},
    {"St. Patricks","Churches",2,64.0f,0.94f,1.0f,0.61f,140.0f,7400.0f,0.42f},
    {"Abravanel Hall","Concert Halls",1,33.0f,0.64f,0.98f,0.44f,100.0f,11200.0f,0.31f},
    {"Ace Hall","Concert Halls",1,20.0f,0.44f,0.88f,0.53f,130.0f,9200.0f,0.26f},
    {"Davies Hall","Concert Halls",1,38.0f,0.69f,1.0f,0.47f,105.0f,10600.0f,0.33f},
    {"Dortmund Concert Hall","Concert Halls",1,30.0f,0.58f,0.96f,0.39f,95.0f,12100.0f,0.3f},
    {"LA Guitar Venue","Concert Halls",3,15.0f,0.32f,0.82f,0.6f,170.0f,8300.0f,0.23f},
    {"LA Piano Hall","Concert Halls",1,26.0f,0.49f,0.91f,0.43f,110.0f,11400.0f,0.27f},
    {"Northwest Orchestral Hall","Concert Halls",1,42.0f,0.72f,0.99f,0.56f,125.0f,9600.0f,0.34f},
    {"NY Piano Hall","Concert Halls",1,24.0f,0.47f,0.9f,0.5f,135.0f,10200.0f,0.26f},
    {"NY String Hall","Concert Halls",1,29.0f,0.54f,0.93f,0.58f,145.0f,9000.0f,0.29f},
    {"Pasadena Taiko Hall","Concert Halls",1,45.0f,0.77f,0.95f,0.38f,80.0f,11900.0f,0.35f},
    {"Reynolds Hall","Concert Halls",1,36.0f,0.65f,0.97f,0.46f,115.0f,10800.0f,0.32f},
    {"San Francisco Hall","Concert Halls",1,31.0f,0.6f,0.94f,0.51f,120.0f,9800.0f,0.3f},
    {"So. Cal. Orchestral Hall","Concert Halls",1,40.0f,0.7f,1.0f,0.42f,105.0f,11600.0f,0.34f},
    {"Malibu Stone Courtyard","Courtyard",3,72.0f,0.31f,0.96f,0.66f,220.0f,7200.0f,0.22f},
    {"Carlsbad Caverns","Caves",2,110.0f,0.98f,1.0f,0.79f,190.0f,4800.0f,0.44f},
    {"Classic Digital","Digital",1,9.0f,0.5f,0.83f,0.3f,85.0f,13900.0f,0.26f},
    {"Digital Hollywood Hall","Digital",1,48.0f,0.89f,1.0f,0.26f,90.0f,14700.0f,0.36f},
    {"Gated Room Reverb","Digital",0,2.0f,0.02f,0.77f,0.32f,160.0f,11800.0f,0.2f},
    {"Planetary Manifestations","Digital",5,165.0f,0.99f,1.0f,0.84f,250.0f,3900.0f,0.46f},
    {"Vocal Digital","Digital",4,28.0f,0.46f,0.88f,0.42f,190.0f,10600.0f,0.27f},
    {"Hamburg Forest","Forests",0,84.0f,0.05f,0.94f,0.91f,280.0f,3600.0f,0.12f},
    {"Hamburg Lake Forest","Forests",3,122.0f,0.19f,1.0f,0.86f,240.0f,4300.0f,0.16f},
    {"War Memorial Opera House","Opera House",1,51.0f,0.86f,1.0f,0.53f,125.0f,9500.0f,0.37f},
    {"SM Parking Garage","Parking Garages",5,68.0f,0.7f,0.91f,0.37f,160.0f,11200.0f,0.32f},
    {"WLA Parking Garage","Parking Garages",5,92.0f,0.83f,0.98f,0.45f,185.0f,9400.0f,0.36f},
    {"Classic Rock Plate","Plates",4,7.0f,0.57f,0.86f,0.34f,170.0f,12300.0f,0.27f},
    {"EW Vintage Plates","Plates",4,13.0f,0.66f,0.82f,0.65f,210.0f,7600.0f,0.29f},
    {"LA Big Drum Rooms","Rock Rooms",3,21.0f,0.61f,0.96f,0.35f,90.0f,12800.0f,0.3f},
    {"LA Small Drum Rooms","Rock Rooms",0,4.0f,0.17f,0.78f,0.4f,140.0f,11900.0f,0.2f},
    {"LA Studio Drum Rooms","Rock Rooms",0,10.0f,0.36f,0.86f,0.46f,120.0f,10800.0f,0.23f},
    {"Solo Vocal","Solo Vocal",4,37.0f,0.39f,0.79f,0.59f,230.0f,8700.0f,0.23f},
    {"LA Scoring Stages","Stages",1,23.0f,0.43f,0.95f,0.41f,100.0f,11700.0f,0.26f},
    {"EastWest Studios","Studios",0,11.0f,0.3f,0.87f,0.51f,125.0f,10200.0f,0.22f},
    {"Berlin Swimming Hall","Swimming Hall",2,27.0f,0.87f,1.0f,0.18f,75.0f,16000.0f,0.38f},
    {"SM Concrete Tank","Tanks",5,17.0f,0.91f,0.89f,0.28f,130.0f,13100.0f,0.37f},
    {"SM Water Tank","Tanks",4,6.0f,0.81f,0.92f,0.21f,110.0f,15100.0f,0.33f},
    {"The Catacombs","The Catacombs",2,94.0f,0.97f,0.95f,0.87f,260.0f,3400.0f,0.43f},
    {"Long Tunnel","Train Stations",5,138.0f,0.96f,1.0f,0.67f,195.0f,6900.0f,0.42f},
    {"Union Station","Train Stations",1,78.0f,0.9f,1.0f,0.43f,140.0f,10300.0f,0.38f},
    {"544 Mateo","Warehouses",3,37.0f,0.76f,0.98f,0.39f,135.0f,11100.0f,0.33f},
    {"ACME Storage","Warehouses",3,44.0f,0.68f,0.91f,0.56f,175.0f,8500.0f,0.31f},
    {"Compton Warehouse","Warehouses",5,57.0f,0.79f,0.96f,0.48f,155.0f,9900.0f,0.35f},
    {"WLA Loading Dock","Warehouses",3,65.0f,0.46f,0.93f,0.63f,210.0f,7500.0f,0.26f},
}};
}
