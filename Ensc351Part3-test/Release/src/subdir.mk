################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Ensc351Part3-test.cpp \
../src/Medium.cpp \
../src/PeerY.cpp \
../src/ReceiverY.cpp \
../src/SenderY.cpp \
../src/myIO.cpp 

OBJS += \
./src/Ensc351Part3-test.o \
./src/Medium.o \
./src/PeerY.o \
./src/ReceiverY.o \
./src/SenderY.o \
./src/myIO.o 

CPP_DEPS += \
./src/Ensc351Part3-test.d \
./src/Medium.d \
./src/PeerY.d \
./src/ReceiverY.d \
./src/SenderY.d \
./src/myIO.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O3 -fsanitize=address       -fsanitize=leak     -fsanitize=undefined       -fsanitize=pointer-compare  -fsanitize=pointer-subtract -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


