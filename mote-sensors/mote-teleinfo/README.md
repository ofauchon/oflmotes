# Dual Teleinfo Power Meter monitoring

1. Introduction 

This project is aimed at monitoring electrical production from solar panels, and 
electricity consumption from house devices.  

2. List of materials

  - Board: Phytec phyWAVE KW22
  - Teleinfo interface : Hallard PitInfo
  - Power meters with Teleinfo interface 


<img src="resources/circuit1.svg" width=200>

3. Introduction to Teleinfo protocol

French power meters have embedded Telinfo interface
( Serial interface : 1200 baud / 7 bit data / 1 bit stop / Parity)

PitInfo interfaces are used to adapt signal levels between the PowerMeters and the UART.

Protocole is quite simple: 

    MOTDETAT 000000 B
    ADCO 031028256512 :
    OPTARIF BASE 0
    ISOUSC 30 9
    BASE 014036028 #
    PTEC TH.. $
    IINST 010 X
    IMAX 037 I
    PAPP 02340 *
`

    => ADCO   => Meter ID
    => IINST  => Current Intensity (Amp)
    => ISOUSC => Max intensity (contract / Amp)
    => BASE   => Total consumed  (Wh)
    => PAAP   => Apparent power
    => PTEC   => Billing period


4. Wiring 

<todo>

5. Flash the board

   make flash 



## References: 

Phywave KW2X:
https://github.com/RIOT-OS/RIOT/wiki/Board%3A-Phytec-phyWAVE-KW22


Linky protocol:
https://www.yadnet.com/domotique/protocole-teleinfo-du-compteur-linky/

Pitinfo interface board:
https://www.tindie.com/products/Hallard/pitinfo/
https://github.com/hallard/teleinfo/tree/master/PiTInfo

