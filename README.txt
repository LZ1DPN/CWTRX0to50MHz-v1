I use a Chinese Forty9-er from eBay , and receiver developed by SM0XGY, in his web site 

http://www.waveguide.se/?article=ne612-receiver-experiment 

Please, read the entire document: Purdum0316-QST-in-Depth-AssemblyManual.pdf

All changes are based on it. And make few modifications in schemes from this pdf,
to adjust real working schemes.  This PDF I've use only to assembly "Chinese" Forty9-er,
because I use other DDS generator (not this from original pdf). Respectively, 
My arduino software sketch is mod on the basis of the original AD7C and VK8BN mod.

In scheme of the Chinese Forty9-er, I finally change LPF windings to run from 0 to 30 MHz 
I remove several windings from L4 coil,  and change two capacitors C17, C18  to 100 pF 
(I can not remember now exactly, but I made LPF from 0 to 30 MHz, with an online calculator).

I remove the consistent LC group (scheme LxCx - 1st mod in top pictures), 
which replaces one quartz and put a wire to bridge to eliminate interruptions in quartz place. 
And I also remove the Capacitor before quartz, to stop frequency filtering.

I replace both end transistors (in driver and PA) in transmitter for Q6 - 8050, I put 2n3904, and
instead Q5 - D882, I put BD139 with sink (not provided in the kit and I added mine).

 
Price:
10$ Forty9-er
4$ for 10 pcs NE612
4$ for LCD
4$ for Arduino Uno
12$ for AD9851
~5$ for knobs, relay, zener and other komponents ...
==========
below 40$
