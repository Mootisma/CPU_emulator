#include "console.h"
#include <chrono>
#include <thread>
#include <iomanip>

void Console::startup(std::string filename) {
    filename_ = filename;
    logger.open("log.txt", std::ios::trunc | std::ios::binary);
    if (!logger.is_open()) {
        std::cerr << "Error opening logger" << std::endl;
    }

    resetSequence();
}

void Console::resetSequence() {
    // clearing RAM with zeros;
    memory.clearRAM();
    
    // opening ROM file
    std::ifstream file;
    file.open(filename_, std::ios::ate);


    // check if file opened successfully
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename_ << std::endl;
        return;
    }

    // saving file size
    rom_size_ = file.tellg();

    // set file pointer to beginning
    file.seekg(0, std::ios::beg);

    std::unique_ptr<char[]> read_rom;
    read_rom = std::make_unique<char[]>(rom_size_);

    // read rom into rom_contents_ and memory (if possible)
    file.read(read_rom.get(), rom_size_);

    // load ROM into memory
    memory.loadROM(read_rom.get(), rom_size_); // put ROM into memory (at address SLUG_ROM_SIZE_);

    // get relevant info from ROM
    address_to_setup = readInt32(ADDRESS_TO_SETUP_);
    address_to_loop = readInt32(ADDRESS_TO_LOOP_);
    load_data_address = readInt32(LOAD_DATA_ADDRESS_);
    program_data_address = readInt32(PROGRAM_DATA_ADDRESS_);
    data_size = readInt32(DATA_SIZE_);
    
    // copy data to RAM (90% sure working)
    for (size_t i=0; i<data_size; i++) {
        uint8_t rom_byte = memory.readByte(load_data_address + i);
        memory.setByte(program_data_address + i, rom_byte);
    }

    // set stack pointer reg to end of stack
    cpu.resetStackPointer();

    // call setup()
    setup();

    // reset stack pointer after setup()
    cpu.resetStackPointer();

    // begin game loop
    loop();
    file.close();
}

void Console::loop() {
    logger << "======= loop() ======= \n";
    cpu.PC = LOOP_CALL_; 
    cpu.initialJAL(address_to_loop);
    std::cout << std::fixed << std::setprecision(20);
    // infinite game loop until exit code
    while (true) { 
        eventLoop();
        uint32_t instruction = readInt32(cpu.PC);

        if (logInstruction) logger << "\nInstruction: " <<std::hex << instruction << std::endl;
        if (logPCLocation) logger << "PC address: " << std::hex << cpu.PC << std::endl;
        
        cpu.PerformInstruction(instruction);
        // reset to start of loop()

        if (cpu.PC <= ZERO_) {
            auto startTime = std::chrono::high_resolution_clock::now();
            gpu.loopIter();
            logger << "\n=== reset loop ===" << std::endl;
            cpu.PC = LOOP_CALL_;
            cpu.initialJAL(address_to_loop);

            double elapsed = 0.0;
            //delay frame
            do {
                auto endTime = std::chrono::high_resolution_clock::now();
                elapsed = std::chrono::duration<double>(endTime - startTime).count();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (elapsed < 0.016667);
        }
            
        // exit condition triggered
        if (exitCondition) exit(EXIT_SUCCESS);
    }
    logger << "\n======= end loop() =======\n";
}

void Console::eventLoop() {
    while( SDL_PollEvent( &eventHandler ) != 0 ) {
        if( eventHandler.type == SDL_QUIT )
        {
            std::cerr << "exit program" << std::endl;
            exitCondition = true;
        }
        //User presses a key
        else if( eventHandler.type == SDL_KEYDOWN )
        {
            //Select surfaces based on key press
            switch( eventHandler.key.keysym.sym )
            {
                case SDLK_UP:
                    controllerByte = controllerByte | CONTROLLER_UP_MASK;
                    break;
                case SDLK_DOWN:
                    controllerByte = controllerByte | CONTROLLER_DOWN_MASK;
                    break;
                case SDLK_LEFT:
                    controllerByte = controllerByte | CONTROLLER_LEFT_MASK;
                    break;
                case SDLK_RIGHT:
                    controllerByte = controllerByte | CONTROLLER_RIGHT_MASK;
                    break;
                case SDLK_a:
                    controllerByte = controllerByte | CONTROLLER_A_MASK;
                    break;
                case SDLK_s:
                    controllerByte = controllerByte | CONTROLLER_B_MASK;
                    break;
                case SDLK_e:
                    controllerByte = controllerByte | CONTROLLER_SELECT_MASK;
                    break;
                case SDLK_SPACE:
                    controllerByte = controllerByte | CONTROLLER_START_MASK;
                    break;
            }
        }
        else if( eventHandler.type == SDL_KEYUP )
        {
            //Select surfaces based on key press
            switch( eventHandler.key.keysym.sym )
            {
                case SDLK_UP:
                    controllerByte = controllerByte & ~CONTROLLER_UP_MASK;
                    break;
                case SDLK_DOWN:
                    controllerByte = controllerByte & ~CONTROLLER_DOWN_MASK;
                    break;
                case SDLK_LEFT:
                    controllerByte = controllerByte & ~CONTROLLER_LEFT_MASK;
                    break;
                case SDLK_RIGHT:
                    controllerByte = controllerByte & ~CONTROLLER_RIGHT_MASK;
                    break;
                case SDLK_a:
                    controllerByte = controllerByte & ~CONTROLLER_A_MASK;
                    break;
                case SDLK_s:
                    controllerByte = controllerByte & ~CONTROLLER_B_MASK;
                    break;
                case SDLK_e:
                    controllerByte = controllerByte & ~CONTROLLER_SELECT_MASK;
                    break;
                case SDLK_SPACE:
                    controllerByte = controllerByte & ~CONTROLLER_START_MASK;
                    break;
            }
        }
    }
}

void Console::setup() {
    logger <<"======= startup() =======\n";
    cpu.PC = LOOP_CALL_; // set PC register to LOOP_CALL_ 0xfffc
    cpu.initialJAL(address_to_setup); // set CPU to run this instruction
    logger << std::hex << cpu.PC << std::endl;
    
    // runs instructions until PC hits 0x0000
    while (cpu.PC != ZERO_) {
        uint32_t instruction = readInt32(cpu.PC);
        
        if (logInstruction) logger << "PC address: " <<std::hex << cpu.PC << std::endl;
        if (logPCLocation) logger << "Instruction: " <<std::hex << instruction << std::endl;

        cpu.PerformInstruction(instruction);
        
        // stop conditions
        if (cpu.PC < SLUG_ROM_SIZE_ || cpu.PC == ZERO_) break;
        if (exitCondition) exit(EXIT_SUCCESS);
    }
    logger <<"======= end startup() =======\n";
}

uint32_t Console::readInt32(const size_t& address) const {
    uint32_t out = 0;
    for (int i = 0; i < 4; i++) {
        out <<= 8;
        out |= (uint8_t)memory.readByte(address+i);
    }
    return out;
}

uint16_t Console::readInt16(const size_t& address) const {
    uint32_t out = 0;
    for (int i = 0; i < 2; i++) {
        out <<= 8;
        out |= (uint8_t)memory.readByte(address+i);
    }
    return out;
}

uint8_t Console::readInt8(const size_t& address) const {
    return (uint8_t)memory.readByte(address);
}

SDL_Event Console::getEventHandler() {
	return eventHandler;
}

bool Console::setExitCondition(bool cond) {
	return exitCondition = cond;
}

int Console::getControllerByte() {
	return controllerByte;
}

SDL_Renderer* Console::getRenderer() {
	return renderer;
}

SDL_Texture* Console::getTexturer() {
	return texture;
}

std::ofstream* Console::getLogger() {
	return loggerP;
}

Memory* Console::getMemory() {
	return memoryP;
}