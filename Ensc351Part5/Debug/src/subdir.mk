################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Ensc351Part5.cpp \
../src/Kvm.cpp \
../src/Medium.cpp \
../src/main.cpp 

OBJS += \
./src/Ensc351Part5.o \
./src/Kvm.o \
./src/Medium.o \
./src/main.o 

CPP_DEPS += \
./src/Ensc351Part5.d \
./src/Kvm.d \
./src/Medium.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -I"/mnt/hgfs/VMsf2020/eclipse-workspace-2021-06/Ensc351ymodLib" -O0 -g3 -Wall -c -fmessage-length=0 -Wno-unused-variable -Wno-unknown-pragmas -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


