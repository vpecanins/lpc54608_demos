# Demos for the LPCXpresso54608

The LPC54608 is a Cortex-M4, power-efficient microcontroller by NXP with plenty 
of peripherals & processing power. The evaluation kit is ([OM13092](https://www.nxp.com/support/developer-resources/hardware-development-tools/lpcxpresso-boards/lpcxpresso-development-board-for-lpc5460x-mcus:OM13092)). 

## How to build

The project is developed using the free, Eclipse-based IDE called 
*MCU Xpresso*. It works with Windows & Linux, but experience says that it works
better on Linux. You can download it ([from NXP website](https://www.nxp.com/support/developer-resources/software-development-tools/mcuxpresso-software-and-tools/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE))

Once you have a local copy of this repository, the standard procedure to open the
project in Eclipse is:

File -> Import -> Existing projects into Workspace -> Select root directory -> Must be the *parent* folder containing the lpc54608_demos directory

Then you can build and flash the project using the blue debug button.

## Organization

The demos are organized in branches of this same repository. You have to do git checkout <branch> to select the project you want to compile.
You can list branches by doing: git branch

## Contact author

- Victor Pecanins <vpecanins@arroweurope.com>

