################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../PeerY.cpp \
../ReceiverY.cpp \
../SenderY.cpp \
../myIO.cpp \
../terminal.cpp \
../yReceiverSS.cpp \
../ySenderSS.cpp 

C_SRCS += \
../crc.c 

OBJS += \
./PeerY.o \
./ReceiverY.o \
./SenderY.o \
./crc.o \
./myIO.o \
./terminal.o \
./yReceiverSS.o \
./ySenderSS.o 

CPP_DEPS += \
./PeerY.d \
./ReceiverY.d \
./SenderY.d \
./myIO.d \
./terminal.d \
./yReceiverSS.d \
./ySenderSS.d 

C_DEPS += \
./crc.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.c subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

myIO.o: ../myIO.cpp subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

yReceiverSS.o: ../yReceiverSS.cpp subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -Wno-unused-variable -Wno-unknown-pragmas -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

ySenderSS.o: ../ySenderSS.cpp subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++17 -I"/mnt/hgfs/VMsf2020/git/ensc351lib/Ensc351" -O0 -g3 -Wall -c -fmessage-length=0 -Wno-unused-variable -Wno-unknown-pragmas -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


