/*******************************************************************************
    OpenAirInterface 
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is 
   included in this distribution in the file called "COPYING". If not, 
   see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@eurecom.fr
  
  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

 *******************************************************************************/
#define MOD_TABLE_SIZE_SHORT 304
#define MOD_TABLE_QPSK_OFFSET 1
#define MOD_TABLE_16QAM_OFFSET 5
#define MOD_TABLE_64QAM_OFFSET 21
#define MOD_TABLE_PSS_OFFSET 85
short mod_table[MOD_TABLE_SIZE_SHORT] = {0,0,768,768,768,-768,-768,768,-768,-768,343,343,343,1030,1030,343,1030,1030,343,-343,343,-1030,1030,-343,1030,-1030,-343,343,-343,1030,-1030,343,-1030,1030,-343,-343,-343,-1030,-1030,-343,-1030,-1030,503,503,503,168,168,503,168,168,503,838,503,1173,168,838,168,1173,838,503,838,168,1173,503,1173,168,838,838,838,1173,1173,838,1173,1173,503,-503,503,-168,168,-503,168,-168,503,-838,503,-1173,168,-838,168,-1173,838,-503,838,-168,1173,-503,1173,-168,838,-838,838,-1173,1173,-838,1173,-1173,-503,503,-503,168,-168,503,-168,168,-503,838,-503,1173,-168,838,-168,1173,-838,503,-838,168,-1173,503,-1173,168,-838,838,-838,1173,-1173,838,-1173,1173,-503,-503,-503,-168,-168,-503,-168,-168,-503,-838,-503,-1173,-168,-838,-168,-1173,-838,-503,-838,-168,-1173,-503,-1173,-168,-838,-838,-838,-1173,-1173,-838,-1173,-1173,1086,0,1081,-108,1065,-215,1038,-320,1001,-422,954,-519,897,-612,832,-698,758,-777,677,-849,589,-912,495,-966,397,-1011,294,-1046,189,-1070,81,-1083,-27,-1086,-135,-1078,-242,-1059,-346,-1030,-447,-990,-543,-941,-634,-882,-719,-814,-796,-739,-866,-656,-927,-566,-979,-471,-1021,-371,-1053,-268,-1074,-162,-1085,-54,-1085,54,-1074,162,-1053,268,-1021,371,-979,471,-927,566,-866,656,-796,739,-719,814,-634,882,-543,941,-447,990,-346,1030,-242,1059,-135,1078,-27,1086,81,1083,189,1070,294,1045,397,1011,495,966,589,912,677,849,758,777,832,698,897,612,954,519,1001,422,1038,320,1065,215,1081,108,16384,0,8192,0,4096,0,2048,0};
