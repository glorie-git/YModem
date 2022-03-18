################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Ensc351Part2.cpp \
../src/Medium.cpp \
../src/PeerY.cpp \
../src/ReceiverY.cpp \
../src/SenderY.cpp \
../src/main.cpp \
../src/myIO.cpp 

OBJS += \
./src/Ensc351Part2.o \
./src/Medium.o \
./src/PeerY.o \
./src/ReceiverY.o \
./src/SenderY.o \
./src/main.o \
./src/myIO.o 

CPP_DEPS += \
./src/Ensc351Part2.d \
./src/Medium.d \
./src/PeerY.d \
./src/ReceiverY.d \
./src/SenderY.d \
./src/main.d \
./src/myIO.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/PeerY.o: ../src/PeerY.cpp src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -Wno-register -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


