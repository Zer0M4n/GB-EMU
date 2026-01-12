#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <memory> // include

class cartridge
{
    private:
        std::vector<uint8_t> ROM ;
        std::string Title;
        std::vector<uint8_t> Entry_Point;
        uint8_t cartridge_type;

    public:
        cartridge(const std::string &path)
        {
            if(loadRom(path))
            {
                parseHeader();
            }
        }
        ~cartridge();
        bool loadRom(const std::string &path)
        {
            std::ifstream rom(path, std::ios::binary | std::ios::ate);

            if (rom.is_open())
            {
                std::streamsize size = rom.tellg();
                ROM.resize(size);

                rom.seekg(0, std::ios::beg);

                if (rom.read(reinterpret_cast<char *>(ROM.data()), size))
                {
                    return true;
                    std::cout << "ROM LOAD , SIZE : " << size << "\n";
                }
            }
            else
            {
                return false;
                printf("ERROR: ROM not found");
            }
        }
        void parseHeader() //fuctions load headers in vectors
        {
            Get_Title();
            cartridgeType();
        }
        void Get_Title()
        {
            for (uint16_t i = 0x0134; i <= 0x0143; i++)
            {
                if (ROM[i] == 0)
                    break;
                Title += static_cast<char>(ROM[i]);

                std::cout << "Titulo " << Title << "\n";
                
            }
        }
        void cartridgeType()
        {
            cartridge_type = ROM[0x147];
        }
        uint8_t read(uint16_t address) 
        {
            // El rango de la ROM en el bus de la Game Boy es 0x0000 a 0x7FFF
            if (address >= 0x0000 && address <= 0x7FFF) {
                // Verificamos que la dirección exista dentro de nuestro vector
                if (address < ROM.size()) {
                    return ROM[address];
                }
            }
            
            // Si la dirección es para la RAM del cartucho (0xA000 - 0xBFFF)
            // Tetris no tiene RAM externa, así que devolvemos un valor por defecto
            if (address >= 0xA000 && address <= 0xBFFF) {
                return 0xFF; // Valor típico cuando no hay nada conectado
            }

            return 0xFF; // "Open bus" o fuera de rango
        }

};
