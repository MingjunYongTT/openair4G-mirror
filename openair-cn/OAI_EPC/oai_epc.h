/** @mainpage

  @section intro Introduction

  openair-mme project tends to provide an implementation of LTE core network.

  @section scope Scope


  @section design Design Philosophy

  Included protocol stacks:
  - SCTP RFC####
  - S1AP 3GPP TS 36.413 R10.5
  - S11 abstraction between MME and S-GW
  - 3GPP TS 23.401 R10.5
  - nw-gtpv1u for s1-u (http://amitchawre.net/)
  - freeDiameter project (http://www.freediameter.net/) 3GPP TS 29.272 R10.5

  @section applications Applications and Usage

  Please use the script to start LTE epc in root src directory

 */

#ifndef OAI_EPC_H_
#define OAI_EPC_H_

int oai_epc_log_specific(int log_level);

int main(int argc, char *argv[]);

#endif /* OAI_EPC_H_ */
