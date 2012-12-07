/*******************************************************************************

  Eurecom OpenAirInterface 2
  Copyright(c) 1999 - 2010 Eurecom

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information
  Openair Admin: openair_admin@eurecom.fr
  Openair Tech : openair_tech@eurecom.fr
  Forums       : http://forums.eurecom.fsr/openairinterface
  Address      : Eurecom, 2229, route des crêtes, 06560 Valbonne Sophia Antipolis, France

*******************************************************************************/

/*! \file rrc_UE.c
* \brief rrc procedures for UE
* \author Raymond Knopp and Navid Nikaein
* \date 2011
* \version 1.0 
* \company Eurecom
* \email: raymond.knopp@eurecom.fr and  navid.nikaein@eurecom.fr
*/ 


#include "defs.h"
#include "extern.h"
#include "RRC/L2_INTERFACE/openair_rrc_L2_interface.h"
#include "LAYER2/RLC/rlc.h"
#include "COMMON/mac_rrc_primitives.h"
#include "UTIL/LOG/log.h"
//#include "RRC/LITE/MESSAGES/asn1_msg.h"
#include "RRCConnectionRequest.h"
#include "RRCConnectionReconfiguration.h"
#include "UL-CCCH-Message.h"
#include "DL-CCCH-Message.h"
#include "UL-DCCH-Message.h"
#include "DL-DCCH-Message.h"
#include "BCCH-DL-SCH-Message.h"
#include "MeasConfig.h"
#include "MeasGapConfig.h"
#include "MeasObjectEUTRA.h"
#include "TDD-Config.h"
#ifdef PHY_ABSTRACTION
#include "OCG.h"
#include "OCG_extern.h"
#endif
#ifdef USER_MODE
#include "RRC/NAS/nas_config.h"
#include "RRC/NAS/rb_config.h"
#endif
#ifdef PHY_EMUL
extern EMULATION_VARS *Emul_vars;
#endif
extern eNB_MAC_INST *eNB_mac_inst;
extern UE_MAC_INST *UE_mac_inst;
extern RSRP_meas_mapping[100];
extern RSRQ_meas_mapping[33];

//extern PHY_VARS_eNB **PHY_vars_eNB_g;
#ifdef BIGPHYSAREA
extern void *bigphys_malloc(int);
#endif

extern inline unsigned int taus(void);

extern timeToTrigger_ms[16];
extern u32 T304[8];
extern **PHY_vars_UE_g;

extern u8 measFlag;

void init_SI_UE(u8 Mod_id,u8 eNB_index) {

  int i;


  UE_rrc_inst[Mod_id].sizeof_SIB1[eNB_index] = 0;
  UE_rrc_inst[Mod_id].sizeof_SI[eNB_index] = 0;

  UE_rrc_inst[Mod_id].SIB1[eNB_index] = (u8 *)malloc16(32);
  UE_rrc_inst[Mod_id].sib1[eNB_index] = (SystemInformationBlockType1_t *)malloc16(sizeof(SystemInformationBlockType1_t));
  UE_rrc_inst[Mod_id].SI[eNB_index] = (u8 *)malloc16(64);

  for (i=0;i<NB_CNX_UE;i++) {
     UE_rrc_inst[Mod_id].si[eNB_index][i] = (SystemInformation_t *)malloc16(sizeof(SystemInformation_t));
  }

  UE_rrc_inst[Mod_id].Info[eNB_index].SIB1Status = 0;
  UE_rrc_inst[Mod_id].Info[eNB_index].SIStatus = 0;


}

/*------------------------------------------------------------------------------*/
char openair_rrc_lite_ue_init(u8 Mod_id, unsigned char eNB_index){
  /*-----------------------------------------------------------------------------*/

  LOG_D(RRC,"[UE %d] INIT State = RRC_IDLE (eNB %d)\n",Mod_id,eNB_index);
  LOG_D(RRC,"[MSC_NEW][FRAME 00000][RRC_UE][MOD %02d][]\n", Mod_id+NB_eNB_INST);

  UE_rrc_inst[Mod_id].Info[eNB_index].State=RRC_IDLE;
  UE_rrc_inst[Mod_id].Info[eNB_index].T300_active = 0;
  UE_rrc_inst[Mod_id].Info[eNB_index].T304_active = 0;
  UE_rrc_inst[Mod_id].Info[eNB_index].T310_active = 0;
  UE_rrc_inst[Mod_id].Info[eNB_index].UE_index=0xffff;
  UE_rrc_inst[Mod_id].Srb0[eNB_index].Active=0;
  UE_rrc_inst[Mod_id].Srb1[eNB_index].Active=0;
  UE_rrc_inst[Mod_id].Srb2[eNB_index].Active=0;

  init_SI_UE(Mod_id,eNB_index);
  LOG_D(RRC,"[UE %d] INIT: phy_sync_2_ch_ind\n", Mod_id);

#ifndef NO_RRM
  send_msg(&S_rrc,msg_rrc_phy_synch_to_CH_ind(Mod_id,eNB_index,UE_rrc_inst[Mod_id].Mac_id));
#endif

#ifdef NO_RRM //init ch SRB0, SRB1 & BDTCH
  openair_rrc_on(Mod_id,0);
#endif

  return 0;
}

long binary_search_int(int elements[], long numElem, int value) {
	long first, last, middle, search;
	first = 0;
	last = numElem-1;
	middle = (first+last)/2;
	if(value < elements[0])
		return first;
	if(value > elements[last])
		return last;

	while (first <= last) {
		if (elements[middle] < value)
			first = middle+1;
		else if (elements[middle] == value) {
			search = middle+1;
			break;
		}
		else
			last = middle -1;

		middle = (first+last)/2;
	}
	if (first > last)
		LOG_E(RRC,"Error in binary search!");
	return search;
}

/* This is a binary search routine which operates on an array of floating
   point numbers and returns the index of the range the value lies in
   Used for RSRP and RSRQ measurement mapping. Can potentially be used for other things
*/
long binary_search_float(float elements[], long numElem, float value) {
	long first, last, middle, search;
	first = 0;
	last = numElem-1;
	middle = (first+last)/2;
	if(value <= elements[0])
		return first;
	if(value >= elements[last])
		return last;

	while (last - first > 1) {
		if (elements[middle] > value)
			last = middle;
		else
			first = middle;

		middle = (first+last)/2;
	}
	if (first < 0 || first >= numElem)
		LOG_E(RRC,"\n Error in binary search float!");
	return first;
}

/*------------------------------------------------------------------------------*/
void rrc_ue_generate_RRCConnectionRequest(u8 Mod_id, u32 frame, u8 eNB_index){
  /*------------------------------------------------------------------------------*/

  u8 i=0,rv[6];

  if(UE_rrc_inst[Mod_id].Srb0[eNB_index].Tx_buffer.payload_size ==0){

    // Get RRCConnectionRequest, fill random for now
    // Generate random byte stream for contention resolution
    for (i=0;i<6;i++) {
      rv[i]=taus()&0xff;
      LOG_T(RRC,"%x.",rv[i]);
    }
    LOG_T(RRC,"\n");
    UE_rrc_inst[Mod_id].Srb0[eNB_index].Tx_buffer.payload_size = do_RRCConnectionRequest((u8 *)UE_rrc_inst[Mod_id].Srb0[eNB_index].Tx_buffer.Payload,rv);

    LOG_I(RRC,"[UE %d] : Frame %d, Logical Channel UL-CCCH (SRB0), Generating RRCConnectionRequest (bytes %d, eNB %d)\n", 
	  Mod_id, frame, UE_rrc_inst[Mod_id].Srb0[eNB_index].Tx_buffer.payload_size, eNB_index);
     
    for (i=0;i<UE_rrc_inst[Mod_id].Srb0[eNB_index].Tx_buffer.payload_size;i++) {
      LOG_T(RRC,"%x.",UE_rrc_inst[Mod_id].Srb0[eNB_index].Tx_buffer.Payload[i]);
    }
    LOG_T(RRC,"\n");
    /*
      UE_rrc_inst[Mod_id].Srb0[Idx].Tx_buffer.Payload[i] = taus()&0xff;

    UE_rrc_inst[Mod_id].Srb0[Idx].Tx_buffer.payload_size =i;
    */

  }
}


mui_t rrc_mui=0;


/*------------------------------------------------------------------------------*/
void rrc_ue_generate_RRCConnectionSetupComplete(u8 Mod_id, u32 frame, u8 eNB_index){
  /*------------------------------------------------------------------------------*/

  u8 buffer[32];
  u8 size;

  size = do_RRCConnectionSetupComplete(buffer);
  
  LOG_I(RRC,"[UE %d][RAPROC] Frame %d : Logical Channel UL-DCCH (SRB1), Generating RRCConnectionSetupComplete (bytes%d, eNB %d)\n",
	Mod_id,frame, size, eNB_index);

   LOG_D(RLC, "[MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- RLC_DATA_REQ/%d Bytes (RRCConnectionSetupComplete to eNB %d MUI %d) --->][RLC][MOD %02d][RB %02d]\n",
                                     frame, Mod_id+NB_eNB_INST, size, eNB_index, rrc_mui, Mod_id+NB_eNB_INST, DCCH);

   //  rrc_rlc_data_req(Mod_id+NB_eNB_INST,frame, 0 ,DCCH,rrc_mui++,0,size,(char*)buffer);
   pdcp_data_req(Mod_id+NB_eNB_INST,frame, 0 ,DCCH,rrc_mui++,0,size,(char*)buffer,1);

}



void rrc_ue_generate_RRCConnectionReconfigurationComplete(u8 Mod_id, u32 frame, u8 eNB_index) {

  u8 buffer[32], size;

  size = do_RRCConnectionReconfigurationComplete(buffer);
  
  LOG_I(RRC,"HO [UE %d] Frame %d : Logical Channel UL-DCCH (SRB1), Generating RRCConnectionReconfigurationComplete (bytes %d, eNB_index %d)\n",
	Mod_id,frame, size, eNB_index);
  LOG_D(RLC, "HO [MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- RLC_DATA_REQ/%d Bytes (RRCConnectionReconfigurationComplete to eNB %d MUI %d) --->][RLC][MOD %02d][RB %02d]\n",
	frame, Mod_id+NB_eNB_INST, size, eNB_index, rrc_mui, Mod_id+NB_eNB_INST, DCCH);
  
  //rrc_rlc_data_req(Mod_id+NB_eNB_INST,frame, 0 ,DCCH,rrc_mui++,0,size,(char*)buffer);
  if(UE_rrc_inst[Mod_id].Info[eNB_index].State == RRC_IDLE && UE_rrc_inst[Mod_id].HandoverInfoUe.targetCellId != 0xFF) {
	  pdcp_data_req(Mod_id+NB_eNB_INST,frame, 0 ,DCCH,rrc_mui++,0,size,(char*)buffer,1);
	  LOG_D(RRC,"\nWatch this!\n");
  }
  else
	  pdcp_data_req(Mod_id+NB_eNB_INST,frame, 0 ,DCCH,rrc_mui++,0,size,(char*)buffer,1);
}


void rrc_ue_generate_MeasurementReport(u8 Mod_id,u8 eNB_index, u32 frame, UE_RRC_INST *UE_rrc_inst, PHY_VARS_UE *phy_vars_ue) {

  u8 buffer[32], size;
  u8 i,j;
  u8 target_eNB_offset;
  MeasId_t measId;
  PhysCellId_t cellId, targetCellId;
  long rsrq_s,rsrp_t,rsrq_t;
  long rsrp_s, nElem, nElem1;
  float tmp, tmp1;
  nElem = 100;
  nElem1 = 33;
  static cframe=0;
  target_eNB_offset = UE_rrc_inst->Info[0].handoverTarget; // eNB_offset of target eNB: used to obtain the mod_id of target eNB

  for (i=0;i<MAX_MEAS_ID;i++) {
	  if (UE_rrc_inst[Mod_id].measReportList[0][i] != NULL) {
		  measId = UE_rrc_inst[Mod_id].measReportList[0][i]->measId;

		  // Note: Values in the meas report have to be the mapped values...to implement binary search for LUT
		  tmp = phy_vars_ue->PHY_measurements.rsrp_filtered[phy_vars_ue->lte_frame_parms.Nid_cell];
		  rsrp_s = binary_search_float(RSRP_meas_mapping,nElem,tmp); //mapped RSRP of serving cell
		  //rsrq_s = binary_search(phy_vars_ue->PHY_measurements.rsrq_filtered[phy_vars_ue->lte_frame_parms.Nid_cell]; //RSRQ of serving cell
		  tmp1 = phy_vars_ue->PHY_measurements.rsrq_filtered[phy_vars_ue->lte_frame_parms.Nid_cell]; //RSRQ of serving cell
		  rsrq_s = binary_search_float(RSRQ_meas_mapping,nElem1,tmp1);//mapped RSRQ of serving cell

		  LOG_D(RRC,"\n binsearch:rsrp_s: %d rsrq_s: %d tmp: %f tmp1: %f \n",rsrp_s,rsrq_s,tmp,tmp1);


		  rsrp_t = binary_search_float(RSRP_meas_mapping,nElem,phy_vars_ue->PHY_measurements.rsrp_filtered[target_eNB_offset]); //RSRP of target cell
		  rsrq_t = binary_search_float(RSRQ_meas_mapping,nElem1,phy_vars_ue->PHY_measurements.rsrq_filtered[target_eNB_offset]); //RSRQ of target cell

		  if (measFlag == 1) {
			  cellId = get_adjacent_cell_id(Mod_id,0); //PhycellId of serving cell
			  targetCellId = UE_rrc_inst[Mod_id].HandoverInfoUe.targetCellId ;//get_adjacent_cell_id(Mod_id,target_eNB_offset); //PhycellId of target cell
			  LOG_D(RRC,"Sending MeasReport: servingCell(%d) targetCell(%d) rsrp_s(%d) rsrq_s(%d) rsrp_t(%d) rsrq_t(%d) \n", \
					  cellId,targetCellId,rsrp_s,rsrq_s,rsrp_t,rsrq_t);

			  size = do_MeasurementReport(buffer,measId,targetCellId,rsrp_s,rsrq_s,rsrp_t,rsrq_t);
			  msg("[RRC][UE %d] Frame %d : Generating Measurement Report\n",Mod_id,Mac_rlc_xface->frame);

		  if (frame != cframe){
		  pdcp_data_req(Mod_id+NB_eNB_INST,frame,0,DCCH,rrc_mui++,0,size,(char*)buffer,1);
		  cframe=frame;
		  LOG_W(PDCP, "[UE %d] Frame %d Sending MeasReport (%d bytes) through DCCH%d to PDCP \n",Mod_id,frame, size, DCCH);
		  }

		  measFlag = 0; //re-setting measFlag so that no more MeasReports are sent in this frame
		  }
  }

}
}

/*------------------------------------------------------------------------------*/
int rrc_ue_decode_ccch(u8 Mod_id, u32 frame, SRB_INFO *Srb_info, u8 eNB_index){
  /*------------------------------------------------------------------------------*/

  DL_CCCH_Message_t dlccchmsg;
  DL_CCCH_Message_t *dl_ccch_msg=&dlccchmsg;
  asn_dec_rval_t dec_rval;
  int i;

  memset(dl_ccch_msg,0,sizeof(DL_CCCH_Message_t));

  //  LOG_D(RRC,"[UE %d] Decoding DL-CCCH message (%d bytes), State %d\n",Mod_id,Srb_info->Rx_buffer.payload_size,
  //	UE_rrc_inst[Mod_id].Info[eNB_index].State);

  dec_rval = uper_decode(NULL,
			 &asn_DEF_DL_CCCH_Message,
			 (void**)&dl_ccch_msg,
	 		 (uint8_t*)Srb_info->Rx_buffer.Payload,
			 100,0,0);

  xer_fprint(stdout,&asn_DEF_DL_CCCH_Message,(void*)dl_ccch_msg);

  if ((dec_rval.code != RC_OK) && (dec_rval.consumed==0)) {
    LOG_E(RRC,"[UE %d] Frame %d : Failed to decode DL-CCCH-Message (%d bytes)\n",Mod_id,dec_rval.consumed);
    return -1;
  }

  if (dl_ccch_msg->message.present == DL_CCCH_MessageType_PR_c1) {

    if (UE_rrc_inst[Mod_id].Info[eNB_index].State == RRC_SI_RECEIVED) {

      switch (dl_ccch_msg->message.choice.c1.present) {

      case DL_CCCH_MessageType__c1_PR_NOTHING :

	LOG_I(RRC,"[UE%d] Frame %d : Received PR_NOTHING on DL-CCCH-Message\n",Mod_id,frame);
	return 0;
	break;
      case DL_CCCH_MessageType__c1_PR_rrcConnectionReestablishment:
          LOG_D(RRC, "[MSC_MSG][FRAME %05d][MAC_UE][MOD %02d][][--- MAC_DATA_IND (rrcConnectionReestablishment ENB %d) --->][RRC_UE][MOD %02d][]\n",
            frame,  Mod_id+NB_eNB_INST, eNB_index,  Mod_id+NB_eNB_INST);

	LOG_I(RRC,"[UE%d] Frame %d : Logical Channel DL-CCCH (SRB0), Received RRCConnectionReestablishment\n",Mod_id,frame);
	return 0;
	break;
      case DL_CCCH_MessageType__c1_PR_rrcConnectionReestablishmentReject:
          LOG_D(RRC, "[MSC_MSG][FRAME %05d][MAC_UE][MOD %02d][][--- MAC_DATA_IND (RRCConnectionReestablishmentReject ENB %d) --->][RRC_UE][MOD %02d][]\n",
            frame,  Mod_id+NB_eNB_INST, eNB_index,  Mod_id+NB_eNB_INST);
	LOG_I(RRC,"[UE%d] Frame %d : Logical Channel DL-CCCH (SRB0), Received RRCConnectionReestablishmentReject\n",Mod_id,frame);
	return 0;
	break;
      case DL_CCCH_MessageType__c1_PR_rrcConnectionReject:
          LOG_D(RRC, "[MSC_MSG][FRAME %05d][MAC_UE][MOD %02d][][--- MAC_DATA_IND (rrcConnectionReject ENB %d) --->][RRC_UE][MOD %02d][]\n",
            frame,  Mod_id+NB_eNB_INST, eNB_index,  Mod_id+NB_eNB_INST);

	LOG_I(RRC,"[UE%d] Frame %d : Logical Channel DL-CCCH (SRB0), Received RRCConnectionReject \n",Mod_id,frame);
	return 0;
	break;
      case DL_CCCH_MessageType__c1_PR_rrcConnectionSetup:
          LOG_D(RRC, "[MSC_MSG][FRAME %05d][MAC_UE][MOD %02d][][--- MAC_DATA_IND (rrcConnectionSetup ENB %d) --->][RRC_UE][MOD %02d][]\n",
            frame,  Mod_id+NB_eNB_INST, eNB_index,  Mod_id+NB_eNB_INST);

	LOG_I(RRC,"[UE%d][RAPROC] Frame %d : Logical Channel DL-CCCH (SRB0), Received RRCConnectionSetup \n",Mod_id,frame);
	// Get configuration

	// Release T300 timer
	UE_rrc_inst[Mod_id].Info[eNB_index].T300_active=0;
	rrc_ue_process_radioResourceConfigDedicated(Mod_id,frame,eNB_index,
						    &dl_ccch_msg->message.choice.c1.choice.rrcConnectionSetup.criticalExtensions.choice.c1.choice.rrcConnectionSetup_r8.radioResourceConfigDedicated);

	rrc_ue_generate_RRCConnectionSetupComplete(Mod_id,frame, eNB_index);

	return 0;
	break;
      default:
	LOG_I(RRC,"[UE%d] Frame %d : Unknown message\n",Mod_id,frame);
	return -1;
      }
    }
  }

  return 0;
}


s32 rrc_ue_establish_srb1(u8 Mod_id,u32 frame,u8 eNB_index,
			 struct SRB_ToAddMod *SRB_config) { // add descriptor from RRC PDU

  u8 lchan_id = DCCH;

  UE_rrc_inst[Mod_id].Srb1[eNB_index].Active = 1;
  UE_rrc_inst[Mod_id].Srb1[eNB_index].Status = RADIO_CONFIG_OK;//RADIO CFG
  UE_rrc_inst[Mod_id].Srb1[eNB_index].Srb_info.Srb_id = 1;

    // copy default configuration for now
  memcpy(&UE_rrc_inst[Mod_id].Srb1[eNB_index].Srb_info.Lchan_desc[0],&DCCH_LCHAN_DESC,LCHAN_DESC_SIZE);
  memcpy(&UE_rrc_inst[Mod_id].Srb1[eNB_index].Srb_info.Lchan_desc[1],&DCCH_LCHAN_DESC,LCHAN_DESC_SIZE);


  LOG_I(RRC,"[UE %d], CONFIG_SRB1 %d corresponding to eNB_index %d\n", Mod_id,lchan_id,eNB_index);
  
  rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_ADD, lchan_id);
  rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_ADD,lchan_id,SIGNALLING_RADIO_BEARER,Rlc_info_am_config);
  //  UE_rrc_inst[Mod_id].Srb1[eNB_index].Srb_info.Tx_buffer.payload_size=DEFAULT_MEAS_IND_SIZE+1;


  return(0);
}

s32 rrc_ue_establish_srb2(u8 Mod_id,u32 frame,u8 eNB_index,
			 struct SRB_ToAddMod *SRB_config) { // add descriptor from RRC PDU

  u8 lchan_id = DCCH1;

  UE_rrc_inst[Mod_id].Srb2[eNB_index].Active = 1;
  UE_rrc_inst[Mod_id].Srb2[eNB_index].Status = RADIO_CONFIG_OK;//RADIO CFG
  UE_rrc_inst[Mod_id].Srb2[eNB_index].Srb_info.Srb_id = 2;

    // copy default configuration for now
  memcpy(&UE_rrc_inst[Mod_id].Srb2[eNB_index].Srb_info.Lchan_desc[0],&DCCH_LCHAN_DESC,LCHAN_DESC_SIZE);
  memcpy(&UE_rrc_inst[Mod_id].Srb2[eNB_index].Srb_info.Lchan_desc[1],&DCCH_LCHAN_DESC,LCHAN_DESC_SIZE);


  LOG_I(RRC,"[UE %d], CONFIG_SRB2 %d corresponding to eNB_index %d\n",Mod_id,lchan_id,eNB_index);

  rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_ADD, lchan_id);
  rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_ADD,lchan_id,SIGNALLING_RADIO_BEARER,Rlc_info_am_config);

  //  UE_rrc_inst[Mod_id].Srb1[eNB_index].Srb_info.Tx_buffer.payload_size=DEFAULT_MEAS_IND_SIZE+1;


  return(0);
}

s32 rrc_ue_establish_drb(u8 Mod_id,u32 frame,u8 eNB_index,
			 struct DRB_ToAddMod *DRB_config) { // add descriptor from RRC PDU
  int oip_ifup=0;
  
    LOG_D(RRC,"[UE] Frame %d: Configuring DRB %ld/LCID %d\n",
      frame,DRB_config->drb_Identity,(int)*DRB_config->logicalChannelIdentity);

  switch (DRB_config->rlc_Config->present) {
  case RLC_Config_PR_NOTHING:
    LOG_D(RRC,"[UE] Frame %d: Received RLC_Config_PR_NOTHING!! for DRB Configuration\n",frame);
    return(-1);
    break;
  case RLC_Config_PR_um_Bi_Directional :
      
    LOG_D(RRC,"[UE %d] Frame %d: Establish RLC UM Bidirectional, DRB %d Active\n", 
	  Mod_id,frame,DRB_config->drb_Identity);
    rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_ADD,
			 (eNB_index * MAX_NUM_RB) + *DRB_config->logicalChannelIdentity);
    rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_ADD,
		       (eNB_index * MAX_NUM_RB) + *DRB_config->logicalChannelIdentity,
		       RADIO_ACCESS_BEARER,Rlc_info_um); 
#ifdef NAS_NETLINK
    LOG_I(OIP,"[UE %d] trying to bring up the OAI interface oai%d\n", Mod_id, oai_emulation.info.nb_enb_local+Mod_id);
    oip_ifup=nas_config(oai_emulation.info.nb_enb_local+Mod_id,// interface index
	       oai_emulation.info.nb_enb_local+Mod_id+1,
	       NB_eNB_INST+Mod_id+1);
    if (oip_ifup == 0 ){ // interface is up --> send a config the DRB
      oai_emulation.info.oai_ifup[Mod_id]=1;
      LOG_I(OIP,"[UE %d] Config the oai%d to send/receive pkt on DRB %d to/from the protocol stack\n",  
	    Mod_id,
	    oai_emulation.info.nb_enb_local+Mod_id,
	    (eNB_index * MAX_NUM_RB) + *DRB_config->logicalChannelIdentity);
	    
	    rb_conf_ipv4(0,//add
			 Mod_id,//cx align with the UE index
			 oai_emulation.info.nb_enb_local+Mod_id,//inst num_enb+ue_index
			 (eNB_index * MAX_NUM_RB) + *DRB_config->logicalChannelIdentity,//rb
			 0,//dscp
			 ipv4_address(oai_emulation.info.nb_enb_local+Mod_id+1,NB_eNB_INST+Mod_id+1),//saddr
			 ipv4_address(oai_emulation.info.nb_enb_local+Mod_id+1,eNB_index+1));//daddr
	    LOG_D(RRC,"[UE %d] State = Attached (eNB %d)\n",Mod_id,eNB_index);	 
    }
#endif
    break;
  case RLC_Config_PR_um_Uni_Directional_UL :
  case RLC_Config_PR_um_Uni_Directional_DL :
  case RLC_Config_PR_am:
    LOG_E(RRC,"[UE] Frame %d: Illegal RLC mode for DRB\n",frame);
    return(-1);
    break;
  }

  return(0);
}


void  rrc_ue_process_measConfig(u8 Mod_id,u8 eNB_index,MeasConfig_t *measConfig){

  // This is the procedure described in 36.331 Section 5.5.2.1
  int i;
  long ind;
  MeasObjectToAddMod_t *measObj;
  MeasObjectEUTRA_t *measObjd;

  if (measConfig->measObjectToRemoveList != NULL) {
    for (i=0;i<measConfig->measObjectToRemoveList->list.count;i++) {
      ind   = *measConfig->measObjectToRemoveList->list.array[i];
      free(UE_rrc_inst[Mod_id].MeasObj[eNB_index][ind-1]);
    }
  }
  if (measConfig->measObjectToAddModList != NULL) {
    LOG_I(RRC,"Measurement Object List is present\n");
    for (i=0;i<measConfig->measObjectToAddModList->list.count;i++) {
      measObj = measConfig->measObjectToAddModList->list.array[i];
      ind   = measConfig->measObjectToAddModList->list.array[i]->measObjectId;

      if (UE_rrc_inst[Mod_id].MeasObj[eNB_index][ind-1]) {
	LOG_I(RRC,"Modifying measurement object %d\n",ind);
	memcpy((char*)UE_rrc_inst[Mod_id].MeasObj[eNB_index][ind-1],
	       (char*)measObj,
	       sizeof(MeasObjectToAddMod_t));
      }
      else {
	LOG_I(RRC,"Adding measurement object %d\n",ind);
	if (measObj->measObject.present == MeasObjectToAddMod__measObject_PR_measObjectEUTRA) {
	  LOG_I(RRC,"EUTRA Measurement : carrierFreq %d, allowedMeasBandwidth %d,presenceAntennaPort1 %d, neighCellConfig %d\n",
		measObj->measObject.choice.measObjectEUTRA.carrierFreq,
		measObj->measObject.choice.measObjectEUTRA.allowedMeasBandwidth,
		measObj->measObject.choice.measObjectEUTRA.presenceAntennaPort1,
		measObj->measObject.choice.measObjectEUTRA.neighCellConfig.buf[0]);
	  UE_rrc_inst[Mod_id].MeasObj[eNB_index][ind-1]=measObj;


	}
      }
    }

    rrc_mac_config_req(Mod_id,0,0,eNB_index,
		       (RadioResourceConfigCommonSIB_t *)NULL,
		       (struct PhysicalConfigDedicated *)NULL,
		       UE_rrc_inst[Mod_id].MeasObj[eNB_index],
		       (MAC_MainConfig_t *)NULL,
		       0,
		       (struct LogicalChannelConfig *)NULL,
		       (MeasGapConfig_t *)NULL,
		       (TDD_Config_t *)NULL,
		       (MobilityControlInfo_t *)NULL,
		       NULL,
		       NULL);
  }
  if (measConfig->reportConfigToRemoveList != NULL) {
    for (i=0;i<measConfig->reportConfigToRemoveList->list.count;i++) {
      ind   = *measConfig->reportConfigToRemoveList->list.array[i];
      free(UE_rrc_inst[Mod_id].ReportConfig[eNB_index][ind-1]);
    }
  }
  if (measConfig->reportConfigToAddModList != NULL) {
    LOG_I(RRC,"Report Configuration List is present\n");
    for (i=0;i<measConfig->reportConfigToAddModList->list.count;i++) {
      ind   = measConfig->reportConfigToAddModList->list.array[i]->reportConfigId;

      if (UE_rrc_inst[Mod_id].ReportConfig[eNB_index][ind-1]) {
	LOG_I(RRC,"Modifying Report Configuration %d\n",ind);
	memcpy((char*)UE_rrc_inst[Mod_id].ReportConfig[eNB_index][ind-1],
	       (char*)measConfig->reportConfigToAddModList->list.array[i],
	       sizeof(ReportConfigToAddMod_t));
      }
      else {
	LOG_I(RRC,"Adding Report Configuration %d\n",ind-1);
	UE_rrc_inst[Mod_id].ReportConfig[eNB_index][ind-1] = measConfig->reportConfigToAddModList->list.array[i];
      }
    }
  }

  if (measConfig->measIdToRemoveList != NULL) {
    for (i=0;i<measConfig->measIdToRemoveList->list.count;i++) {
      ind   = *measConfig->measIdToRemoveList->list.array[i];
      free(UE_rrc_inst[Mod_id].MeasId[eNB_index][ind-1]);
    }
  }

  if (measConfig->measIdToAddModList != NULL) {
    for (i=0;i<measConfig->measIdToAddModList->list.count;i++) {
      ind   = measConfig->measIdToAddModList->list.array[i]->measId;
      if (UE_rrc_inst[Mod_id].MeasId[eNB_index][ind-1]) {
	LOG_I(RRC,"Modifying Measurement ID %d\n",ind);
	memcpy((char*)UE_rrc_inst[Mod_id].MeasId[eNB_index][ind-1],
	       (char*)measConfig->measIdToAddModList->list.array[i],
	       sizeof(MeasIdToAddMod_t));
      }
      else {
	LOG_I(RRC,"Adding Measurement ID %d\n",ind);
	UE_rrc_inst[Mod_id].MeasId[eNB_index][ind-1] = measConfig->measIdToAddModList->list.array[i];
      }
    }
  }

  if (measConfig->measGapConfig !=NULL) {
    if (UE_rrc_inst[Mod_id].measGapConfig[eNB_index]) {
      memcpy((char*)UE_rrc_inst[Mod_id].measGapConfig[eNB_index],(char*)measConfig->measGapConfig,
	     sizeof(MeasGapConfig_t));
    }
    else {
      UE_rrc_inst[Mod_id].measGapConfig[eNB_index] = measConfig->measGapConfig;
    }
  }

  if (measConfig->quantityConfig != NULL) {
    if (UE_rrc_inst[Mod_id].QuantityConfig[eNB_index]) {
      LOG_I(RRC,"Modifying Quantity Configuration \n");
      memcpy((char*)UE_rrc_inst[Mod_id].QuantityConfig[eNB_index],(char*)measConfig->quantityConfig,
	     sizeof(QuantityConfig_t));
    }
    else {
      LOG_I(RRC,"Adding Quantity configuration\n");
      UE_rrc_inst[Mod_id].QuantityConfig[eNB_index] = measConfig->quantityConfig;
    }

    UE_rrc_inst[Mod_id].filter_coeff_rsrp = 1./pow(2,(*UE_rrc_inst[Mod_id].QuantityConfig[eNB_index]->quantityConfigEUTRA->filterCoefficientRSRP)/4);
    UE_rrc_inst[Mod_id].filter_coeff_rsrq = 1./pow(2,(*UE_rrc_inst[Mod_id].QuantityConfig[eNB_index]->quantityConfigEUTRA->filterCoefficientRSRQ)/4);

    LOG_I(RRC,"UE %d rsrp-coeff: %d rsrq-coeff: %d rsrp_factor: %f rsrq_factor: %f \n",
    		UE_rrc_inst[Mod_id].Info[eNB_index].UE_index,
    		*UE_rrc_inst[Mod_id].QuantityConfig[eNB_index]->quantityConfigEUTRA->filterCoefficientRSRP,
    		*UE_rrc_inst[Mod_id].QuantityConfig[eNB_index]->quantityConfigEUTRA->filterCoefficientRSRQ,
    		UE_rrc_inst[Mod_id].filter_coeff_rsrp, UE_rrc_inst[Mod_id].filter_coeff_rsrp,
    		UE_rrc_inst[Mod_id].filter_coeff_rsrp, UE_rrc_inst[Mod_id].filter_coeff_rsrq);
  }

  if (measConfig->s_Measure != NULL) {
    UE_rrc_inst[Mod_id].s_measure = *measConfig->s_Measure;
  }

  if (measConfig->speedStatePars != NULL) {
	  if (UE_rrc_inst[Mod_id].speedStatePars)
		  memcpy((char*)UE_rrc_inst[Mod_id].speedStatePars,(char*)measConfig->speedStatePars,sizeof(struct MeasConfig__speedStatePars));
	  else
		  UE_rrc_inst[Mod_id].speedStatePars = measConfig->speedStatePars;
	  LOG_I(RRC,"Configuring mobility optimization params for UE %d \n",UE_rrc_inst[Mod_id].Info[0].UE_index);
  }
}


void	rrc_ue_process_radioResourceConfigDedicated(u8 Mod_id,u32 frame, u8 eNB_index,
						    RadioResourceConfigDedicated_t *radioResourceConfigDedicated) {

  long SRB_id,DRB_id;
  int i,cnt;
  LogicalChannelConfig_t *SRB1_logicalChannelConfig,*SRB2_logicalChannelConfig;

  // Save physicalConfigDedicated if present
  if (radioResourceConfigDedicated->physicalConfigDedicated) {
    if (UE_rrc_inst[Mod_id].physicalConfigDedicated[eNB_index]) {
      memcpy((char*)UE_rrc_inst[Mod_id].physicalConfigDedicated[eNB_index],(char*)radioResourceConfigDedicated->physicalConfigDedicated,
	     sizeof(struct PhysicalConfigDedicated));

    }
    else {
      UE_rrc_inst[Mod_id].physicalConfigDedicated[eNB_index] = radioResourceConfigDedicated->physicalConfigDedicated;
    }
  }
  // Apply macMainConfig if present
  if (radioResourceConfigDedicated->mac_MainConfig) {
    if (radioResourceConfigDedicated->mac_MainConfig->present == RadioResourceConfigDedicated__mac_MainConfig_PR_explicitValue) {
      if (UE_rrc_inst[Mod_id].mac_MainConfig[eNB_index]) {
	memcpy((char*)UE_rrc_inst[Mod_id].mac_MainConfig[eNB_index],(char*)&radioResourceConfigDedicated->mac_MainConfig->choice.explicitValue,
	       sizeof(MAC_MainConfig_t));
      }
      else
	UE_rrc_inst[Mod_id].mac_MainConfig[eNB_index] = &radioResourceConfigDedicated->mac_MainConfig->choice.explicitValue;
    }
  }

  // Apply spsConfig if present
  if (radioResourceConfigDedicated->sps_Config) {
    if (UE_rrc_inst[Mod_id].sps_Config[eNB_index]) {
      memcpy(UE_rrc_inst[Mod_id].sps_Config[eNB_index],radioResourceConfigDedicated->sps_Config,
	     sizeof(struct SPS_Config));
    }
    else {
      UE_rrc_inst[Mod_id].sps_Config[eNB_index] = radioResourceConfigDedicated->sps_Config;
    }
  }
  // Establish SRBs if present
  // loop through SRBToAddModList
  if (radioResourceConfigDedicated->srb_ToAddModList) {

    for (cnt=0;cnt<radioResourceConfigDedicated->srb_ToAddModList->list.count;cnt++) {

      SRB_id = radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt]->srb_Identity;
      LOG_D(RRC,"[UE %d]: Frame %d SRB config cnt %d (SRB%ld)\n",Mod_id,frame,cnt,SRB_id);
      if (SRB_id == 1) {
	if (UE_rrc_inst[Mod_id].SRB1_config[eNB_index]) {
	  memcpy(UE_rrc_inst[Mod_id].SRB1_config[eNB_index],radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt],
		 sizeof(struct SRB_ToAddMod));
	}
	else {
	  UE_rrc_inst[Mod_id].SRB1_config[eNB_index] = radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt];

	  rrc_ue_establish_srb1(Mod_id,frame,eNB_index,radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt]);
	  if (UE_rrc_inst[Mod_id].SRB1_config[eNB_index]->logicalChannelConfig) {
	    if (UE_rrc_inst[Mod_id].SRB1_config[eNB_index]->logicalChannelConfig->present == SRB_ToAddMod__logicalChannelConfig_PR_explicitValue) {
	      SRB1_logicalChannelConfig = &UE_rrc_inst[Mod_id].SRB1_config[eNB_index]->logicalChannelConfig->choice.explicitValue;
	    }
	    else {
	      SRB1_logicalChannelConfig = &SRB1_logicalChannelConfig_defaultValue;
	    }
	  }
	  else {
	    SRB1_logicalChannelConfig = &SRB1_logicalChannelConfig_defaultValue;
	  }


      LOG_D(RRC, "[MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- MAC_CONFIG_REQ  (SRB1 eNB %d) --->][MAC_UE][MOD %02d][]\n",
            frame, Mod_id, eNB_index, Mod_id);
	  rrc_mac_config_req(Mod_id,0,0,eNB_index,
			     (RadioResourceConfigCommonSIB_t *)NULL,
			     UE_rrc_inst[Mod_id].physicalConfigDedicated[eNB_index],
			     (MeasObjectToAddMod_t **)NULL,
			     UE_rrc_inst[Mod_id].mac_MainConfig[eNB_index],
			     1,
			     SRB1_logicalChannelConfig,
			     (MeasGapConfig_t *)NULL,
			     NULL,
			     (MobilityControlInfo_t *)NULL,
			     NULL,
			     NULL);
	}
      }
      else {
	if (UE_rrc_inst[Mod_id].SRB2_config[eNB_index]) {
	  memcpy(UE_rrc_inst[Mod_id].SRB2_config[eNB_index],radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt],
		 sizeof(struct SRB_ToAddMod));
	}
	else {

	  UE_rrc_inst[Mod_id].SRB2_config[eNB_index] = radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt];

	  rrc_ue_establish_srb2(Mod_id,frame,eNB_index,radioResourceConfigDedicated->srb_ToAddModList->list.array[cnt]);
	  if (UE_rrc_inst[Mod_id].SRB2_config[eNB_index]->logicalChannelConfig) {
	    if (UE_rrc_inst[Mod_id].SRB2_config[eNB_index]->logicalChannelConfig->present == SRB_ToAddMod__logicalChannelConfig_PR_explicitValue){
	      LOG_I(RRC,"Applying Explicit SRB2 logicalChannelConfig\n");
	      SRB2_logicalChannelConfig = &UE_rrc_inst[Mod_id].SRB2_config[eNB_index]->logicalChannelConfig->choice.explicitValue;
	    }
	    else {
	      LOG_I(RRC,"Applying default SRB2 logicalChannelConfig\n");
	      SRB2_logicalChannelConfig = &SRB2_logicalChannelConfig_defaultValue;
	    }
	  }
	  else {
	    SRB2_logicalChannelConfig = &SRB2_logicalChannelConfig_defaultValue;
	  }

      LOG_D(RRC, "[MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- MAC_CONFIG_REQ  (SRB2 eNB %d) --->][MAC_UE][MOD %02d][]\n",
            frame, Mod_id, eNB_index, Mod_id);
      rrc_mac_config_req(Mod_id,0,0,eNB_index,
			 (RadioResourceConfigCommonSIB_t *)NULL,
			 UE_rrc_inst[Mod_id].physicalConfigDedicated[eNB_index],
			 (MeasObjectToAddMod_t **)NULL,
			 UE_rrc_inst[Mod_id].mac_MainConfig[eNB_index],
			 2,
			 SRB2_logicalChannelConfig,
			 UE_rrc_inst[Mod_id].measGapConfig[eNB_index],
			 (TDD_Config_t *)NULL,
			 (MobilityControlInfo_t *)NULL,
			 (u8 *)NULL,
			 (u16 *)NULL);
	}
      }
    }
  }

  // Establish DRBs if present
  if (radioResourceConfigDedicated->drb_ToAddModList) {

    for (i=0;i<radioResourceConfigDedicated->drb_ToAddModList->list.count;i++) {
      DRB_id   = radioResourceConfigDedicated->drb_ToAddModList->list.array[i]->drb_Identity-1;
      if (UE_rrc_inst[Mod_id].DRB_config[eNB_index][DRB_id]) {
	memcpy(UE_rrc_inst[Mod_id].DRB_config[eNB_index][DRB_id],radioResourceConfigDedicated->drb_ToAddModList->list.array[i],
	       sizeof(struct DRB_ToAddMod));
      }
      else {
	UE_rrc_inst[Mod_id].DRB_config[eNB_index][DRB_id] = radioResourceConfigDedicated->drb_ToAddModList->list.array[i];

	rrc_ue_establish_drb(Mod_id,frame,eNB_index,radioResourceConfigDedicated->drb_ToAddModList->list.array[i]);
	// MAC/PHY Configuration
	LOG_D(RRC, "[MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- MAC_CONFIG_REQ (DRB %d eNB %d) --->][MAC_UE][MOD %02d][]\n",
	      frame, Mod_id, DRB_id, eNB_index, Mod_id);
	rrc_mac_config_req(Mod_id,0,0,eNB_index,
			   (RadioResourceConfigCommonSIB_t *)NULL,
			   UE_rrc_inst[Mod_id].physicalConfigDedicated[eNB_index],
			   (MeasObjectToAddMod_t **)NULL,
			   UE_rrc_inst[Mod_id].mac_MainConfig[eNB_index],
			   *UE_rrc_inst[Mod_id].DRB_config[eNB_index][DRB_id]->logicalChannelIdentity,
			   UE_rrc_inst[Mod_id].DRB_config[eNB_index][DRB_id]->logicalChannelConfig,
			   UE_rrc_inst[Mod_id].measGapConfig[eNB_index],
			   (TDD_Config_t*)NULL,
			   (MobilityControlInfo_t *)NULL,
			   (u8 *)NULL,
			   (u16 *)NULL);
	
      }
    }
  }

  UE_rrc_inst[Mod_id].Info[eNB_index].State = RRC_CONNECTED;
  LOG_D(RRC,"[UE %d] State = RRC_CONNECTED (eNB %d)\n",Mod_id,eNB_index);
  

}


void rrc_ue_process_rrcConnectionReconfiguration(u8 Mod_id, u32 frame,
						 RRCConnectionReconfiguration_t *rrcConnectionReconfiguration,
						 u8 eNB_index) {

  LOG_I(RRC,"[UE %d] Frame %d: Receiving from SRB1 (DL-DCCH), Processing RRCConnectionReconfiguration (eNB %d)\n",
	Mod_id,frame,eNB_index);
  if (rrcConnectionReconfiguration->criticalExtensions.present == RRCConnectionReconfiguration__criticalExtensions_PR_c1) {
    if (rrcConnectionReconfiguration->criticalExtensions.choice.c1.present == RRCConnectionReconfiguration__criticalExtensions__c1_PR_rrcConnectionReconfiguration_r8) {

      if (rrcConnectionReconfiguration->criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.mobilityControlInfo) {
    	  LOG_I(RRC,"Mobility Control Information is present\n");
    	  rrc_ue_process_mobilityControlInfo(Mod_id,frame,eNB_index,
					   rrcConnectionReconfiguration->criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.mobilityControlInfo);
      }
      if (rrcConnectionReconfiguration->criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.measConfig != NULL) {
    	  LOG_I(RRC,"Measurement Configuration is present\n");
    	  rrc_ue_process_measConfig(Mod_id,eNB_index,
				  rrcConnectionReconfiguration->criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.measConfig);
      }
      if (rrcConnectionReconfiguration->criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.radioResourceConfigDedicated) {
    	  LOG_I(RRC,"Radio Resource Configuration is present\n");
    	  rrc_ue_process_radioResourceConfigDedicated(Mod_id,frame,eNB_index,
						    rrcConnectionReconfiguration->criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.radioResourceConfigDedicated);
      }
    } // c1 present
  } // critical extensions present
}

/* 36.331, 5.3.5.4	Reception of an RRCConnectionReconfiguration including the mobilityControlInfo by the UE (handover) */
void	rrc_ue_process_mobilityControlInfo(u8 Mod_id, u32 frame, u8 eNB_index, struct MobilityControlInfo *mobilityControlInfo) {

	if(UE_rrc_inst[Mod_id].Info[eNB_index].T310_active == 1)
		UE_rrc_inst[Mod_id].Info[eNB_index].T310_active = 0;
	UE_rrc_inst[Mod_id].Info[eNB_index].T304_active = 1;
	UE_rrc_inst[Mod_id].Info[eNB_index].T304_cnt = T304[mobilityControlInfo->t304];

	//Removing SRB1 and SRB2 and DRB0
	rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_REMOVE, Mod_id+DCCH);
	rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_REMOVE,Mod_id+DCCH,SIGNALLING_RADIO_BEARER,Rlc_info_am_config);

	rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_REMOVE, Mod_id+DCCH1);
	rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_REMOVE,Mod_id+DCCH1,SIGNALLING_RADIO_BEARER,Rlc_info_am_config);

	rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_REMOVE, Mod_id+DTCH);
	rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_REMOVE,Mod_id+DTCH,RADIO_ACCESS_BEARER,Rlc_info_um);

	//A little cleanup at RRC...
	//Copying current queue config to free RRC index
	/*
	memcpy((void *)UE_rrc_inst[Mod_id].SRB1_config[~(7<<eNB_index)],(void *)UE_rrc_inst[Mod_id].SRB1_config[7<<eNB_index],sizeof(SRB_ToAddMod_t));
	memcpy((void *)UE_rrc_inst[Mod_id].SRB2_config[~(7<<eNB_index)],(void *)UE_rrc_inst[Mod_id].SRB2_config[7<<eNB_index],sizeof(SRB_ToAddMod_t));
	memcpy((void *)UE_rrc_inst[Mod_id].DRB_config[~(7<<eNB_index)][0],(void *)UE_rrc_inst[Mod_id].DRB_config[7<<eNB_index][0],sizeof(DRB_ToAddMod_t));
	 */
	//Freeing current queue config..
	free((void *)UE_rrc_inst[Mod_id].SRB1_config[eNB_index]);
	free((void *)UE_rrc_inst[Mod_id].SRB2_config[eNB_index]);
	free((void *)UE_rrc_inst[Mod_id].DRB_config[eNB_index][0]);

	UE_rrc_inst[Mod_id].SRB1_config[eNB_index] = NULL;
	UE_rrc_inst[Mod_id].SRB2_config[eNB_index] = NULL;
	UE_rrc_inst[Mod_id].DRB_config[eNB_index][0] = NULL;

	//Synchronisation to DL of target cell
    LOG_D(RRC, "HO: Reset PDCP and RLC for configured RBs.. \n[MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- MAC_CONFIG_REQ  (SRB2 eNB %d) --->][MAC_UE][MOD %02d][]\n",
          frame, Mod_id, eNB_index, Mod_id);

    // Reset MAC and configure PHY
    rrc_mac_config_req(Mod_id,0,0,eNB_index,
			 (RadioResourceConfigCommonSIB_t *)NULL,
			 (struct PhysicalConfigDedicated *)NULL,
			 (MeasObjectToAddMod_t **)NULL,
			 (MAC_MainConfig_t *)NULL,
			 0,
			 (struct LogicalChannelConfig *)NULL,
			 (MeasGapConfig_t *)NULL,
			 (TDD_Config_t *)NULL,
			 mobilityControlInfo,
			 (u8 *)NULL,
			 (u16 *)NULL);

    // Re-establish PDCP for all RBs that are established
//	rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_ADD, Mod_id+DCCH);
//	rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_ADD, Mod_id+DCCH1);
//	rrc_pdcp_config_req (Mod_id+NB_eNB_INST, frame, 0, ACTION_ADD, Mod_id+DTCH);


    // Re-establish RLC for all RBs that are established
//	rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_ADD,Mod_id+DCCH,SIGNALLING_RADIO_BEARER,Rlc_info_am_config);
//	rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_ADD,Mod_id+DCCH1,SIGNALLING_RADIO_BEARER,Rlc_info_am_config);
//	rrc_rlc_config_req(Mod_id+NB_eNB_INST,frame,0,ACTION_ADD,Mod_id+DTCH,RADIO_ACCESS_BEARER,Rlc_info_um);


    UE_rrc_inst[Mod_id].Info[eNB_index].State = RRC_SI_RECEIVED;
}

void rrc_detach_from_eNB(u8 Mod_id,u8 eNB_index) {
	//UE_rrc_inst[Mod_id].DRB_config[eNB_index]
}

/*------------------------------------------------------------------------------------------*/
void  rrc_ue_decode_dcch(u8 Mod_id,u32 frame,u8 Srb_id, u8 *Buffer,u8 eNB_index){
  /*------------------------------------------------------------------------------------------*/

  u8 Mod_id_t; //handover
  DL_DCCH_Message_t dldcchmsg;
  DL_DCCH_Message_t *dl_dcch_msg=&dldcchmsg;
  //  asn_dec_rval_t dec_rval;
  int i;

  if (Srb_id != 1) {
    LOG_D(RRC,"[UE %d] Frame %d: Received message on DL-DCCH (SRB1), should not have ...\n",Mod_id,frame);
    return;
  }

  memset(dl_dcch_msg,0,sizeof(DL_DCCH_Message_t));

  // decode messages
  //  LOG_D(RRC,"[UE %d] Decoding DL-DCCH message\n",Mod_id);
  /*
  for (i=0;i<30;i++)
    LOG_T(RRC,"%x.",Buffer[i]);
  LOG_T(RRC, "\n");
  */
  uper_decode(NULL,
	      &asn_DEF_DL_DCCH_Message,
	      (void**)&dl_dcch_msg,
	      (uint8_t*)Buffer,
	      RRC_BUF_SIZE,0,0);

  xer_fprint(stdout,&asn_DEF_DL_DCCH_Message,(void*)dl_dcch_msg);
  if (dl_dcch_msg->message.present == DL_DCCH_MessageType_PR_c1) {

    if (UE_rrc_inst[Mod_id].Info[eNB_index].State == RRC_CONNECTED) {

      switch (dl_dcch_msg->message.choice.c1.present) {

      case DL_DCCH_MessageType__c1_PR_NOTHING :
	LOG_I("[UE %d] Frame %d : Received PR_NOTHING on DL-DCCH-Message\n",Mod_id,frame);
	return;
	break;
      case DL_DCCH_MessageType__c1_PR_csfbParametersResponseCDMA2000:
	break;
      case DL_DCCH_MessageType__c1_PR_dlInformationTransfer:
	break;
      case DL_DCCH_MessageType__c1_PR_handoverFromEUTRAPreparationRequest:
	break;
      case DL_DCCH_MessageType__c1_PR_mobilityFromEUTRACommand:
	break;
      case DL_DCCH_MessageType__c1_PR_rrcConnectionReconfiguration:
    	  if(dl_dcch_msg->message.choice.c1.choice.rrcConnectionReconfiguration.criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.mobilityControlInfo != NULL) {
    		  /*
    		   * 36.331, 5.3.5.4 Reception of an RRCConnectionReconfiguration including the mobilityControlInfo by the UE (handover)
    		   */
    		  if(UE_rrc_inst[Mod_id].HandoverInfoUe.targetCellId != dl_dcch_msg->message.choice.c1.choice.rrcConnectionReconfiguration.criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.mobilityControlInfo->targetPhysCellId) {
    			  LOG_W(RRC,"\nHandover target (%d) is different from RSRP measured target (%d)..\n",
    					  dl_dcch_msg->message.choice.c1.choice.rrcConnectionReconfiguration.criticalExtensions.choice.c1.choice.rrcConnectionReconfiguration_r8.mobilityControlInfo->targetPhysCellId,
    					  UE_rrc_inst[Mod_id].HandoverInfoUe.targetCellId);
    		  }
    		  else {
    			  Mod_id_t = get_adjacent_cell_mod_id(UE_rrc_inst[Mod_id].HandoverInfoUe.targetCellId);
    			  if(Mod_id_t != 0xFF) {
    				  LOG_D(RRC,"Received RRCReconfig with mobilityControlInfo \n");
    				  rrc_ue_process_rrcConnectionReconfiguration(Mod_id,frame,&dl_dcch_msg->message.choice.c1.choice.rrcConnectionReconfiguration,eNB_index);
    				  //Initiate RA procedure
    				  //PHY_vars_UE_g[UE_id]->UE_mode[0] = PRACH
    				  rrc_ue_generate_RRCConnectionReconfigurationComplete(Mod_id,frame,Mod_id_t);
    			  }
    		  }
    	  }
    	  else {
    		  rrc_ue_process_rrcConnectionReconfiguration(Mod_id,frame,&dl_dcch_msg->message.choice.c1.choice.rrcConnectionReconfiguration,eNB_index);
    		  rrc_ue_generate_RRCConnectionReconfigurationComplete(Mod_id,frame,eNB_index);
    	  }
	break;
      case DL_DCCH_MessageType__c1_PR_rrcConnectionRelease:
	break;
      case DL_DCCH_MessageType__c1_PR_securityModeCommand:
	break;
      case DL_DCCH_MessageType__c1_PR_ueCapabilityEnquiry:
	break;
      case DL_DCCH_MessageType__c1_PR_counterCheck:
	break;
#ifdef Rel10
      case DL_DCCH_MessageType__c1_PR_ueInformationRequest_r9:
	break;
      case DL_DCCH_MessageType__c1_PR_loggedMeasurementConfiguration_r10:
	break;
      case DL_DCCH_MessageType__c1_PR_rnReconfiguration_r10:
	break;
#endif
      case DL_DCCH_MessageType__c1_PR_spare1:
      case DL_DCCH_MessageType__c1_PR_spare2:
      case DL_DCCH_MessageType__c1_PR_spare3:
      case DL_DCCH_MessageType__c1_PR_spare4:
	break;
      }
    }
  }
#ifndef NO_RRM
    send_msg(&S_rrc,msg_rrc_end_scan_req(Mod_id,eNB_index));
#endif
}

const char siWindowLength[7][5] = {"1ms\0","2ms\0","5ms\0","10ms\0","15ms\0","20ms\0","40ms\0"};
const char siWindowLength_int[7] = {1,2,5,10,15,20,40};

const char SIBType[16][6] ={"SIB3\0","SIB4\0","SIB5\0","SIB6\0","SIB7\0","SIB8\0","SIB9\0","SIB10\0","SIB11\0","SIB12\0","SIB13\0","Sp2\0","Sp3\0","Sp4\0"};
const char SIBPeriod[7][7]= {"80ms\0","160ms\0","320ms\0","640ms\0","1280ms\0","2560ms\0","5120ms\0"};

int decode_BCCH_DLSCH_Message(u8 Mod_id,u32 frame,u8 eNB_index,u8 *Sdu,u8 Sdu_len) {

  BCCH_DL_SCH_Message_t bcch_message;
  BCCH_DL_SCH_Message_t *bcch_message_ptr=&bcch_message;
  SystemInformationBlockType1_t **sib1=&UE_rrc_inst[Mod_id].sib1[eNB_index];
  SystemInformation_t **si=UE_rrc_inst[Mod_id].si[eNB_index];
  asn_dec_rval_t dec_rval;
  u32 si_window;

  memset(&bcch_message,0,sizeof(BCCH_DL_SCH_Message_t));
  //  LOG_D(RRC,"[UE %d] Decoding DL_BCCH_DLSCH_Message\n",Mod_id)
  dec_rval = uper_decode_complete(NULL,
				  &asn_DEF_BCCH_DL_SCH_Message,
				  (void **)&bcch_message_ptr,
				  (const void *)Sdu,
				  Sdu_len);//,0,0);

  if ((dec_rval.code != RC_OK) && (dec_rval.consumed==0)) {
    LOG_E(RRC,"[UE %d] Failed to decode BCCH_DLSCH_MESSAGE (%d bits)\n",Mod_id,dec_rval.consumed);
    return -1;
  }  
  //  xer_fprint(stdout,  &asn_DEF_BCCH_DL_SCH_Message, (void*)&bcch_message);

  if (bcch_message.message.present == BCCH_DL_SCH_MessageType_PR_c1) {
    switch (bcch_message.message.choice.c1.present) {
    case BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1:
      if ((frame %2) == 0) {
	if (UE_rrc_inst[Mod_id].Info[eNB_index].SIB1Status == 0) {
	  memcpy((void*)*sib1,
		 (void*)&bcch_message.message.choice.c1.choice.systemInformationBlockType1,
		 sizeof(SystemInformationBlockType1_t));
	  LOG_D(RRC,"[UE %d] Decoding First SIB1\n",Mod_id);
	  decode_SIB1(Mod_id,eNB_index);
	}
	break;
    case BCCH_DL_SCH_MessageType__c1_PR_systemInformation:
      if ((UE_rrc_inst[Mod_id].Info[eNB_index].SIB1Status == 1) &&
	  (UE_rrc_inst[Mod_id].Info[eNB_index].SIStatus == 0)) {

	si_window = (frame%UE_rrc_inst[Mod_id].Info[eNB_index].SIperiod)/frame%UE_rrc_inst[Mod_id].Info[eNB_index].SIwindowsize;
	memcpy((void*)si[si_window],
	       (void*)&bcch_message.message.choice.c1.choice.systemInformation,
	       sizeof(SystemInformation_t));
	LOG_D(RRC,"[UE %d] Decoding SI for frame %d, si_window %d\n",Mod_id,frame,si_window);
	decode_SI(Mod_id,frame,eNB_index,si_window);
      }
      break;
      case BCCH_DL_SCH_MessageType__c1_PR_NOTHING:
	break;
      }
    }
  }
}	    
	    

int decode_SIB1(u8 Mod_id,u8 eNB_index) {
  asn_dec_rval_t dec_rval;
  SystemInformationBlockType1_t **sib1=&UE_rrc_inst[Mod_id].sib1[eNB_index];
  int i;

  LOG_D(RRC,"[UE %d] : Dumping SIB 1 (%d bits)\n",Mod_id,dec_rval.consumed);

  //  xer_fprint(stdout,&asn_DEF_SystemInformationBlockType1, (void*)*sib1);

  msg("cellAccessRelatedInfo.cellIdentity : %x.%x.%x.%x\n",
      (*sib1)->cellAccessRelatedInfo.cellIdentity.buf[0],
      (*sib1)->cellAccessRelatedInfo.cellIdentity.buf[1],
      (*sib1)->cellAccessRelatedInfo.cellIdentity.buf[2],
      (*sib1)->cellAccessRelatedInfo.cellIdentity.buf[3]);

  msg("cellSelectionInfo.q_RxLevMin       : %d\n",(int)(*sib1)->cellSelectionInfo.q_RxLevMin);
  msg("freqBandIndicator                  : %d\n",(int)(*sib1)->freqBandIndicator);
  msg("siWindowLength                     : %s\n",siWindowLength[(*sib1)->si_WindowLength]);
  if ((*sib1)->schedulingInfoList.list.count>0) {
    for (i=0;i<(*sib1)->schedulingInfoList.list.count;i++) {
      msg("siSchedulingInfoPeriod[%d]          : %s\n",i,SIBPeriod[(int)(*sib1)->schedulingInfoList.list.array[i]->si_Periodicity]);
      if ((*sib1)->schedulingInfoList.list.array[i]->sib_MappingInfo.list.count>0)
	msg("siSchedulingInfoSIBType[%d]         : %s\n",i,SIBType[(int)(*(*sib1)->schedulingInfoList.list.array[i]->sib_MappingInfo.list.array[0])]);
      else {
	msg("mapping list %d is null\n",i);
      }
    }
  }
  else {
    msg("siSchedulingInfoPeriod[0]          : PROBLEM!!!\n");
   return -1;
  }

  if ((*sib1)->tdd_Config)
    msg("TDD subframe assignment            : %d\nS-Subframe Config                  : %d\n",(int)(*sib1)->tdd_Config->subframeAssignment,(int)(*sib1)->tdd_Config->specialSubframePatterns);
  

  UE_rrc_inst[Mod_id].Info[eNB_index].SIperiod     = 8<<((int)(*sib1)->si_WindowLength);
  UE_rrc_inst[Mod_id].Info[eNB_index].SIwindowsize = siWindowLength_int[(*sib1)->si_WindowLength];
  LOG_D(RRC, "[MSC_MSG][FRAME unknown][RRC_UE][MOD %02d][][--- MAC_CONFIG_REQ (SIB1 params eNB %d) --->][MAC_UE][MOD %02d][]\n",
             Mod_id, eNB_index, Mod_id);
  rrc_mac_config_req(Mod_id,0,0,eNB_index,
		     (RadioResourceConfigCommonSIB_t *)NULL,
		     (struct PhysicalConfigDedicated *)NULL,
		     (MeasObjectToAddMod_t **)NULL,
		     (MAC_MainConfig_t *)NULL,
		     0,
		     (struct LogicalChannelConfig *)NULL,
		     (MeasGapConfig_t *)NULL,
		     UE_rrc_inst[Mod_id].sib1[eNB_index]->tdd_Config,
		     (MobilityControlInfo_t *)NULL,
		     &UE_rrc_inst[Mod_id].Info[eNB_index].SIwindowsize,
		     &UE_rrc_inst[Mod_id].Info[eNB_index].SIperiod);
  
  UE_rrc_inst[Mod_id].Info[eNB_index].SIB1Status = 1;
  return 0;

}


void dump_sib2(SystemInformationBlockType2_t *sib2) {

  LOG_D(RRC,"radioResourceConfigCommon.rach_ConfigCommon.preambleInfo.numberOfRA_Preambles : %ld\n",
      sib2->radioResourceConfigCommon.rach_ConfigCommon.preambleInfo.numberOfRA_Preambles);

  //  if (radioResourceConfigCommon.rach_ConfigCommon.preambleInfo.preamblesGroupAConfig)
  //msg("radioResourceConfigCommon.rach_ConfigCommon.preambleInfo.preamblesGroupAConfig ",sib2->radioResourceConfigCommon.rach_ConfigCommon.preambleInfo.preamblesGroupAConfig = NULL;

  LOG_D(RRC,"[UE]radioResourceConfigCommon.rach_ConfigCommon.powerRampingParameters.powerRampingStep : %ld\n",sib2->radioResourceConfigCommon.rach_ConfigCommon.powerRampingParameters.powerRampingStep);

  LOG_D(RRC,"[UE]radioResourceConfigCommon.rach_ConfigCommon.powerRampingParameters.preambleInitialReceivedTargetPower : %ld\n",sib2->radioResourceConfigCommon.rach_ConfigCommon.powerRampingParameters.preambleInitialReceivedTargetPower);

  LOG_D(RRC,"radioResourceConfigCommon.rach_ConfigCommon.ra_SupervisionInfo.preambleTransMax  : %ld\n",sib2->radioResourceConfigCommon.rach_ConfigCommon.ra_SupervisionInfo.preambleTransMax);

  LOG_D(RRC,"radioResourceConfigCommon.rach_ConfigCommon.ra_SupervisionInfo.ra_ResponseWindowSize : %ld\n",sib2->radioResourceConfigCommon.rach_ConfigCommon.ra_SupervisionInfo.ra_ResponseWindowSize);

  LOG_D(RRC,"radioResourceConfigCommon.rach_ConfigCommon.ra_SupervisionInfo.mac_ContentionResolutionTimer : %ld\n",sib2->radioResourceConfigCommon.rach_ConfigCommon.ra_SupervisionInfo.mac_ContentionResolutionTimer);

  LOG_D(RRC,"radioResourceConfigCommon.rach_ConfigCommon.maxHARQ_Msg3Tx : %ld\n",
      sib2->radioResourceConfigCommon.rach_ConfigCommon.maxHARQ_Msg3Tx);

  LOG_D(RRC,"radioResourceConfigCommon.prach_Config.rootSequenceIndex : %ld\n",sib2->radioResourceConfigCommon.prach_Config.rootSequenceIndex);
  LOG_D(RRC,"radioResourceConfigCommon.prach_Config.prach_ConfigInfo.prach_ConfigIndex : %ld\n",sib2->radioResourceConfigCommon.prach_Config.prach_ConfigInfo.prach_ConfigIndex);
  LOG_D(RRC,"radioResourceConfigCommon.prach_Config.prach_ConfigInfo.highSpeedFlag : %d\n",  (int)sib2->radioResourceConfigCommon.prach_Config.prach_ConfigInfo.highSpeedFlag);
  LOG_D(RRC,"radioResourceConfigCommon.prach_Config.prach_ConfigInfo.zeroCorrelationZoneConfig : %ld\n",  sib2->radioResourceConfigCommon.prach_Config.prach_ConfigInfo.zeroCorrelationZoneConfig);
  LOG_D(RRC,"radioResourceConfigCommon.prach_Config.prach_ConfigInfo.prach_FreqOffset %ld\n",  sib2->radioResourceConfigCommon.prach_Config.prach_ConfigInfo.prach_FreqOffset);

  // PDSCH-Config
  LOG_D(RRC,"radioResourceConfigCommon.pdsch_ConfigCommon.referenceSignalPower  : %ld\n",sib2->radioResourceConfigCommon.pdsch_ConfigCommon.referenceSignalPower);
  LOG_D(RRC,"radioResourceConfigCommon.pdsch_ConfigCommon.p_b : %ld\n",sib2->radioResourceConfigCommon.pdsch_ConfigCommon.p_b);

  // PUSCH-Config
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.n_SB  : %ld\n",sib2->radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.n_SB);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.hoppingMode  : %ld\n",sib2->radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.hoppingMode);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.pusch_HoppingOffset : %ld\n",sib2->radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.pusch_HoppingOffset);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.enable64QAM : %d\n",(int)sib2->radioResourceConfigCommon.pusch_ConfigCommon.pusch_ConfigBasic.enable64QAM);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.groupHoppingEnabled : %d\n",(int)sib2->radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.groupHoppingEnabled);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.groupAssignmentPUSCH : %ld\n",sib2->radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.groupAssignmentPUSCH);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.sequenceHoppingEnabled : %d\n",(int)sib2->radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.sequenceHoppingEnabled);
  LOG_D(RRC,"radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.cyclicShift : %ld\n",sib2->radioResourceConfigCommon.pusch_ConfigCommon.ul_ReferenceSignalsPUSCH.cyclicShift);

  // PUCCH-Config

  LOG_D(RRC,"radioResourceConfigCommon.pucch_ConfigCommon.deltaPUCCH_Shift : %ld\n",sib2->radioResourceConfigCommon.pucch_ConfigCommon.deltaPUCCH_Shift);
  LOG_D(RRC,"radioResourceConfigCommon.pucch_ConfigCommon.nRB_CQI : %ld\n",sib2->radioResourceConfigCommon.pucch_ConfigCommon.nRB_CQI);
  LOG_D(RRC,"radioResourceConfigCommon.pucch_ConfigCommon.nCS_AN : %ld\n",sib2->radioResourceConfigCommon.pucch_ConfigCommon.nCS_AN);
  LOG_D(RRC,"radioResourceConfigCommon.pucch_ConfigCommon.n1PUCCH_AN : %ld\n",sib2->radioResourceConfigCommon.pucch_ConfigCommon.n1PUCCH_AN);

  LOG_D(RRC,"radioResourceConfigCommon.soundingRS_UL_ConfigCommon.present : %d\n",sib2-> radioResourceConfigCommon.soundingRS_UL_ConfigCommon.present);


  // uplinkPowerControlCommon

  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.p0_NominalPUSCH : %ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.p0_NominalPUSCH);
  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.alpha : %ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.alpha);

  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.p0_NominalPUCCH : %ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.p0_NominalPUCCH);
  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format1 : %ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format1);
  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format1b :%ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format1b);
  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format2  :%ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format2);
  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format2a :%ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format2a);
  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format2b :%ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.deltaFList_PUCCH.deltaF_PUCCH_Format2b);

  LOG_D(RRC,"radioResourceConfigCommon.uplinkPowerControlCommon.deltaPreambleMsg3 : %ld\n",sib2->radioResourceConfigCommon.uplinkPowerControlCommon.deltaPreambleMsg3);

  LOG_D(RRC,"radioResourceConfigCommon.ul_CyclicPrefixLength : %ld\n", sib2->radioResourceConfigCommon.ul_CyclicPrefixLength);

  LOG_D(RRC,"ue_TimersAndConstants.t300 : %ld\n", sib2->ue_TimersAndConstants.t300);
  LOG_D(RRC,"ue_TimersAndConstants.t301 : %ld\n", sib2->ue_TimersAndConstants.t301);
  LOG_D(RRC,"ue_TimersAndConstants.t310 : %ld\n", sib2->ue_TimersAndConstants.t310);
  LOG_D(RRC,"ue_TimersAndConstants.n310 : %ld\n", sib2->ue_TimersAndConstants.n310);
  LOG_D(RRC,"ue_TimersAndConstants.t311 : %ld\n", sib2->ue_TimersAndConstants.t311);
  LOG_D(RRC,"ue_TimersAndConstants.n311 : %ld\n", sib2->ue_TimersAndConstants.n311);

  LOG_D(RRC,"freqInfo.additionalSpectrumEmission : %ld\n",sib2->freqInfo.additionalSpectrumEmission);
  LOG_D(RRC,"freqInfo.ul_CarrierFreq : %d\n",(int)sib2->freqInfo.ul_CarrierFreq);
  LOG_D(RRC,"freqInfo.ul_Bandwidth : %d\n",(int)sib2->freqInfo.ul_Bandwidth);
  LOG_D(RRC,"mbsfn_SubframeConfigList : %d\n",(int)sib2->mbsfn_SubframeConfigList);
  LOG_D(RRC,"timeAlignmentTimerCommon : %ld\n",sib2->timeAlignmentTimerCommon);



}

void dump_sib3(SystemInformationBlockType3_t *sib3) {

}

#ifdef Rel10
void dump_sib13(SystemInformationBlockType13_r9_t *sib13) {

  LOG_D(RRC,"[RRC][UE] Dumping SIB13\n");
  LOG_D(RRC,"[RRC][UE] dumping sib13 second time\n");
  LOG_D(RRC,"NotificationRepetitionCoeff-r9 : %ld\n", sib13->notificationConfig_r9.notificationRepetitionCoeff_r9);
  LOG_D(RRC,"NotificationOffset-r9 : %d\n", (int)sib13->notificationConfig_r9.notificationOffset_r9);
  LOG_D(RRC,"notificationSF-Index-r9 : %d\n", (int)sib13->notificationConfig_r9.notificationSF_Index_r9);

}
#endif
//const char SIBPeriod[7][7]= {"80ms\0","160ms\0","320ms\0","640ms\0","1280ms\0","2560ms\0","5120ms\0"};
int decode_SI(u8 Mod_id,u32 frame,u8 eNB_index,u8 si_window) {

  SystemInformation_t **si=&UE_rrc_inst[Mod_id].si[eNB_index][si_window];
  int i;
  struct SystemInformation_r8_IEs__sib_TypeAndInfo__Member *typeandinfo;
  
  /*
  LOG_D(RRC,"[UE %d] Frame %d : Dumping SI from window %d (%d bytes)\n",Mod_id,Mac_rlc_xface->frame,si_window,dec_rval.consumed);
  for (i=0;i<30;i++)
    LOG_D(RRC,"%x.",UE_rrc_inst[Mod_id].SI[eNB_index][i]);
  LOG_D(RRC,"\n");
  */
  // Dump contents
  if ((*si)->criticalExtensions.present==SystemInformation__criticalExtensions_PR_systemInformation_r8) {
    LOG_D(RRC,"(*si)->criticalExtensions.choice.systemInformation_r8.sib_TypeAndInfo.list.count %d\n",
       (*si)->criticalExtensions.choice.systemInformation_r8.sib_TypeAndInfo.list.count);
  }
  else {
    LOG_D(RRC,"[UE] Unknown criticalExtension version (not Rel8)\n");
    return -1;
  }
  
  for (i=0;i<(*si)->criticalExtensions.choice.systemInformation_r8.sib_TypeAndInfo.list.count;i++) {
    printf("SI count %d\n",i);
    typeandinfo=(*si)->criticalExtensions.choice.systemInformation_r8.sib_TypeAndInfo.list.array[i];

    switch(typeandinfo->present) {
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib2:
      UE_rrc_inst[Mod_id].sib2[eNB_index] = &typeandinfo->choice.sib2;
      LOG_D(RRC,"[UE %d] Frame %d Found SIB2 from eNB %d\n",Mod_id,frame,eNB_index);
      dump_sib2(UE_rrc_inst[Mod_id].sib2[eNB_index]);
      LOG_D(RRC, "[MSC_MSG][FRAME %05d][RRC_UE][MOD %02d][][--- MAC_CONFIG_REQ (SIB2 params  eNB %d) --->][MAC_UE][MOD %02d][]\n",
            frame, Mod_id, eNB_index, Mod_id);
      rrc_mac_config_req(Mod_id,0,0,eNB_index,
			 &UE_rrc_inst[Mod_id].sib2[eNB_index]->radioResourceConfigCommon,
			 (struct PhysicalConfigDedicated *)NULL,
			 (MeasObjectToAddMod_t **)NULL,
			 (MAC_MainConfig_t *)NULL,
			 0,
			 (struct LogicalChannelConfig *)NULL,
			 (MeasGapConfig_t *)NULL,
			 (TDD_Config_t *)NULL,
			 (MobilityControlInfo_t *)NULL,
			 NULL,
			 NULL);
      UE_rrc_inst[Mod_id].Info[eNB_index].SIStatus = 1;
      // After SI is received, prepare RRCConnectionRequest
      rrc_ue_generate_RRCConnectionRequest(Mod_id,frame,eNB_index);

      if (UE_rrc_inst[Mod_id].Info[eNB_index].State == RRC_IDLE) {
	LOG_I(RRC,"[UE %d] Received SIB1/SIB2/SIB3 Switching to RRC_SI_RECEIVED\n",Mod_id);
	UE_rrc_inst[Mod_id].Info[eNB_index].State = RRC_SI_RECEIVED;
      }
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib3:
      UE_rrc_inst[Mod_id].sib3[eNB_index] = &typeandinfo->choice.sib3;
      LOG_I(RRC,"[UE %d] Frame %d Found SIB3 from eNB %d\n",Mod_id,frame,eNB_index);
      dump_sib3(UE_rrc_inst[Mod_id].sib3[eNB_index]);
      UE_rrc_inst[Mod_id].Info[eNB_index].SIStatus = 1;
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib4:
      UE_rrc_inst[Mod_id].sib4[eNB_index] = &typeandinfo->choice.sib4;
      LOG_I(RRC,"[UE %d] Frame %d Found SIB4 from eNB %d\n",Mod_id,frame,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib5:
      UE_rrc_inst[Mod_id].sib5[eNB_index] = &typeandinfo->choice.sib5;
      LOG_I(RRC,"[UE %d] Found SIB5 from eNB %d\n",Mod_id,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib6:
      UE_rrc_inst[Mod_id].sib6[eNB_index] = &typeandinfo->choice.sib6;
      LOG_I(RRC,"[UE %d] Found SIB6 from eNB %d\n",Mod_id,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib7:
      UE_rrc_inst[Mod_id].sib7[eNB_index] = &typeandinfo->choice.sib7;
      LOG_I(RRC,"[UE %d] Found SIB7 from eNB %d\n",Mod_id,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib8:
      UE_rrc_inst[Mod_id].sib8[eNB_index] = &typeandinfo->choice.sib8;
      LOG_I(RRC,"[UE %d] Found SIB8 from eNB %d\n",Mod_id,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib9:
      UE_rrc_inst[Mod_id].sib9[eNB_index] = &typeandinfo->choice.sib9;
      LOG_I(RRC,"[UE %d] Found SIB9 from eNB %d\n",Mod_id,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib10:
      UE_rrc_inst[Mod_id].sib10[eNB_index] = &typeandinfo->choice.sib10;
      LOG_I(RRC,"[UE %d] Found SIB10 from eNB %d\n",Mod_id,eNB_index);
      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib11:
      UE_rrc_inst[Mod_id].sib11[eNB_index] = &typeandinfo->choice.sib11;
      LOG_I(RRC,"[UE %d] Found SIB11 from eNB %d\n",Mod_id,eNB_index);
      break;
#ifdef Rel10
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib12_v920:
      UE_rrc_inst[Mod_id].sib12[eNB_index] = &typeandinfo->choice.sib12_v920;
      LOG_I(RRC,"[RRC][UE %d] Found SIB12 from eNB %d\n",Mod_id,eNB_index);

      break;
    case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib13_v920:
      UE_rrc_inst[Mod_id].sib13[eNB_index] = &typeandinfo->choice.sib13_v920;
      LOG_I(RRC,"[RRC][UE %d] Found SIB13 from eNB %d\n",Mod_id,eNB_index);
      dump_sib13(UE_rrc_inst[Mod_id].sib13[eNB_index]);
      /*
      Mac_rlc_xface->rrc_mac_config_req(Mod_id,0,0,eNB_index,
					&UE_rrc_inst[Mod_id].sib2[eNB_index]->radioResourceConfigCommon,
					(struct PhysicalConfigDedicated *)NULL,
					(MAC_MainConfig_t *)NULL,
					0,
					(struct LogicalChannelConfig *)NULL,
					(MeasGapConfig_t *)NULL,
					(TDD_Config_t *)NULL,
					NULL,
					NULL);
      */
      break;
#endif
    default:
      break;
    }

  }

  return 0;
}

// layer 3 filtering of RSRP (EUTRA) measurements: 36.331, Sec. 5.5.3.2
void ue_meas_filtering(s32 UE_id, UE_RRC_INST *UE_rrc_inst, PHY_VARS_UE *phy_vars_ue, u8 abstraction_flag) {
	float a = UE_rrc_inst->filter_coeff_rsrp; // 'a' in 36.331 Sec. 5.5.3.2
	float a1 = UE_rrc_inst->filter_coeff_rsrq;
	float rsrp_db, rsrq_db;
	u8 eNB_offset;

    if(UE_rrc_inst->QuantityConfig[0] != NULL) { // Only consider 1 serving cell (index: 0)
    	if (UE_rrc_inst->QuantityConfig[0]->quantityConfigEUTRA != NULL) {
    		if(UE_rrc_inst->QuantityConfig[0]->quantityConfigEUTRA->filterCoefficientRSRP != NULL) {

    		for (eNB_offset = 0;eNB_offset<1+phy_vars_ue->PHY_measurements.n_adj_cells;eNB_offset++) {
    			//filter_factor = 1/power(2,*UE_rrc_inst[phy_vars_ue->Mod_id].QuantityConfig[0]->quantityConfigEUTRA->filterCoefficientRSRP/4);
    			if (abstraction_flag == 0) {
    				rsrp_db = (dB_fixed_times10(phy_vars_ue->PHY_measurements.rsrp[eNB_offset])/10.0)-phy_vars_ue->rx_total_gain_dB-dB_fixed(phy_vars_ue->lte_frame_parms.N_RB_DL*12);

    				phy_vars_ue->PHY_measurements.rsrp_filtered[eNB_offset] = (1.0-a)*phy_vars_ue->PHY_measurements.rsrp_filtered[eNB_offset] + \
    				        					a*rsrp_db;

    			}
    			LOG_I(PHY,"UE RRC meas RSRP: eNB_offset: %d rsrp_coef: %3.2f filter_coef: %d before L3 filtering: rsrp: %3.1f \t after L3 filtering: rsrp: %3.1f \n ",
    					eNB_offset,
    					a,
    					*UE_rrc_inst->QuantityConfig[0]->quantityConfigEUTRA->filterCoefficientRSRP,
    					rsrp_db,
    					phy_vars_ue->PHY_measurements.rsrp_filtered[eNB_offset]);
    		}
    	}
    	}
    	else
        	memcpy((void *)phy_vars_ue->PHY_measurements.rsrp_filtered, (void *)phy_vars_ue->PHY_measurements.rsrp, 7*sizeof(int));

    	if (UE_rrc_inst->QuantityConfig[0]->quantityConfigEUTRA != NULL) {
    		if(UE_rrc_inst->QuantityConfig[0]->quantityConfigEUTRA->filterCoefficientRSRQ != NULL) {

    		for (eNB_offset = 0;eNB_offset<1+phy_vars_ue->PHY_measurements.n_adj_cells;eNB_offset++) {
    			if (abstraction_flag == 0) {
    				rsrq_db = (10*log10(phy_vars_ue->PHY_measurements.rsrq[eNB_offset]))-20;
    				phy_vars_ue->PHY_measurements.rsrq_filtered[eNB_offset] = (1-a1)*phy_vars_ue->PHY_measurements.rsrq_filtered[eNB_offset] + \
    					a1*rsrq_db;
    			}
    			LOG_I(PHY,"UE RRC meas RSRQ: eNB_offset: %d rsrq_coef: %3.2f filter_coef: %d before L3 filtering: rsrq: %3.1f \t after L3 filtering: rsrq: %3.1f \n ",
    					eNB_offset,
    					a1,
    					*UE_rrc_inst->QuantityConfig[0]->quantityConfigEUTRA->filterCoefficientRSRQ,
    					rsrq_db,
    					/* phy_vars_ue->PHY_measurements.rsrq[eNB_offset], */
    					phy_vars_ue->PHY_measurements.rsrq_filtered[eNB_offset]);
    		}
    }
    }
    else
    	memcpy((void *)phy_vars_ue->PHY_measurements.rsrq_filtered, (void *)phy_vars_ue->PHY_measurements.rsrq, 7*sizeof(int));

  }
}


u8 check_trigger_meas_event(u8 i /* Mod_id of serving cell */, u8 j /* Meas Index */, UE_RRC_INST *UE_rrc_inst, PHY_VARS_UE *phy_vars_ue, Q_OffsetRange_t ofn, Q_OffsetRange_t ocn, Hysteresis_t hys, Q_OffsetRange_t ofs, Q_OffsetRange_t ocs, long a3_offset, TimeToTrigger_t ttt) {
	u8 eNB_offset;
	PHY_MEASUREMENTS *phy_meas = &phy_vars_ue->PHY_measurements;
	u8 currentCellIndex = phy_vars_ue->lte_frame_parms.Nid_cell;

	LOG_I(RRC,"ofn(%d) ocn(%d) hys(%d) ofs(%d) ocs(%d) a3_offset(%d) ttt(%d) \n", \
				ofn,ocn,hys,ofs,ocs,a3_offset,ttt);

	for (eNB_offset = 1;eNB_offset<1+phy_meas->n_adj_cells;eNB_offset++) {
		/* RHS: Verify that idx 0 corresponds to currentCellIndex in rsrp array */
		if(phy_meas->rsrp_filtered[eNB_offset]+ofn+ocn-hys > phy_meas->rsrp_filtered[0]+ofs+ocs+a3_offset) {
			UE_rrc_inst->measTimer[i][j][eNB_offset-1] += 2; //Called every subframe = 2ms
			LOG_D(RRC,"Entry measTimer[%d][%d]: %d currentCell: %d betterCell: %d \n", \
					i,j,UE_rrc_inst->measTimer[i][j][eNB_offset-1],currentCellIndex,eNB_offset);
		}
		else {
			UE_rrc_inst->measTimer[i][j][eNB_offset-1] = 0; //Exit condition: Resetting the measurement timer
			LOG_D(RRC,"Exit measTimer[%d][%d]: %d currentCell: %d betterCell: %d \n", \
					i,j,UE_rrc_inst->measTimer[i][j][eNB_offset-1],currentCellIndex,eNB_offset);
		}
		if (UE_rrc_inst->measTimer[i][j][eNB_offset-1] >= ttt) {
			UE_rrc_inst->HandoverInfoUe.targetCellId = get_adjacent_cell_id(i,eNB_offset-1); //check this!
			LOG_D(RRC,"\n Handover triggered for UE: targetCellId: %d currentCellId: %d eNB_offset: %d\n",UE_rrc_inst->HandoverInfoUe.targetCellId,i,eNB_offset);
			UE_rrc_inst->Info[0].handoverTarget = eNB_offset;
			return 1;
		}
 	}
	return 0;
}


// Measurement report triggering, described in 36.331 Section 5.5.4.1
void ue_measurement_report_triggering(u8 Mod_id, u32 frame, UE_RRC_INST *UE_rrc_inst) {
	u8 i,j;
	Hysteresis_t	 hys;
	TimeToTrigger_t	 ttt_ms;
	Q_OffsetRange_t ofn;
	Q_OffsetRange_t ocn;
	Q_OffsetRange_t ofs;
	Q_OffsetRange_t ocs;
	long			a3_offset;
	MeasObjectToAddMod_t measObj;
	//MeasId_t	 measId;
	MeasObjectId_t	 measObjId;
	ReportConfigId_t	 reportConfigId;

	for(i=0 ; i<NB_CNX_UE ; i++) {
		for(j=0 ; j<MAX_MEAS_ID ; j++) {
			if(UE_rrc_inst[Mod_id].MeasId[i][j] != NULL) {
			measObjId = UE_rrc_inst[Mod_id].MeasId[i][j]->measObjectId;
			reportConfigId = UE_rrc_inst[Mod_id].MeasId[i][j]->reportConfigId;
			if( /* UE_rrc_inst[Mod_id].MeasId[i][j] != NULL && */ UE_rrc_inst[Mod_id].MeasObj[i][measObjId-1] != NULL) {
				if(UE_rrc_inst[Mod_id].MeasObj[i][measObjId-1]->measObject.present == MeasObjectToAddMod__measObject_PR_measObjectEUTRA) {

					/* consider any neighboring cell detected on the associated frequency to be
					 * applicable when the concerned cell is not included in the blackCellsToAddModList
					 * defined within the VarMeasConfig for this measId */

					//if()
					if(UE_rrc_inst[Mod_id].ReportConfig[i][reportConfigId-1]->reportConfig.present == ReportConfigToAddMod__reportConfig_PR_reportConfigEUTRA && \
							UE_rrc_inst[Mod_id].ReportConfig[i][reportConfigId-1]->reportConfig.choice.reportConfigEUTRA.triggerType.present == \
								ReportConfigEUTRA__triggerType_PR_event) {
							hys = UE_rrc_inst[Mod_id].ReportConfig[i][reportConfigId-1]->reportConfig.choice.reportConfigEUTRA.triggerType.choice.event.hysteresis;
							//Check below line for segfault :)
							ttt_ms = timeToTrigger_ms[UE_rrc_inst[Mod_id].ReportConfig[i][reportConfigId-1]->reportConfig.choice.reportConfigEUTRA.triggerType.choice.event.timeToTrigger];
							// Freq specific offset of neighbor cell freq
							ofn = (UE_rrc_inst->MeasObj[i][measObjId-1]->measObject.choice.measObjectEUTRA.offsetFreq != NULL ? \
									*UE_rrc_inst->MeasObj[i][measObjId-1]->measObject.choice.measObjectEUTRA.offsetFreq : 15 /* Default */);
							// cellIndividualOffset of neighbor cell - not defined yet
							ocn = 0;
							// Freq specific offset of serving cell freq
							ofs = (UE_rrc_inst->MeasObj[i][measObjId-1]->measObject.choice.measObjectEUTRA.offsetFreq != NULL ? \
									*UE_rrc_inst->MeasObj[i][measObjId-1]->measObject.choice.measObjectEUTRA.offsetFreq : 15 /* Default */);
							// cellIndividualOffset of serving cell - not defined yet
							ocs = 0;
							a3_offset = UE_rrc_inst[Mod_id].ReportConfig[i][reportConfigId-1]->reportConfig.choice.reportConfigEUTRA.triggerType.choice.event.eventId.choice.eventA3.a3_Offset;

							switch (UE_rrc_inst[Mod_id].ReportConfig[i][reportConfigId-1]->reportConfig.choice.reportConfigEUTRA.triggerType.choice.event.eventId.present) {
								case ReportConfigEUTRA__triggerType__event__eventId_PR_eventA1:
									break;
								case ReportConfigEUTRA__triggerType__event__eventId_PR_eventA2:
									break;
								case ReportConfigEUTRA__triggerType__event__eventId_PR_eventA3:
									if (check_trigger_meas_event(i,j,&UE_rrc_inst[Mod_id],PHY_vars_UE_g[Mod_id],ofn,ocn,hys,ofs,ocs,a3_offset,ttt_ms) && \
											UE_rrc_inst[Mod_id].Info[0].State == RRC_CONNECTED && \
											UE_rrc_inst[Mod_id].Info[0].T304_active == 0) {
										//trigger measurement reporting procedure (36.331, section 5.5.5)
										if (UE_rrc_inst[Mod_id].measReportList[i][j] == NULL) {
											UE_rrc_inst[Mod_id].measReportList[i][j] = malloc(sizeof(MEAS_REPORT_LIST));
										}
										UE_rrc_inst[Mod_id].measReportList[i][j]->measId = UE_rrc_inst[Mod_id].MeasId[i][j]->measId;
										UE_rrc_inst[Mod_id].measReportList[i][j]->numberOfReportsSent = 0;
										rrc_ue_generate_MeasurementReport(Mod_id,0,frame,&UE_rrc_inst[Mod_id],PHY_vars_UE_g[Mod_id]);
										LOG_I(RRC,"\n A3 event detected for UE %d in state: %d \n", (int)Mod_id, UE_rrc_inst[Mod_id].Info[0].State);
									}
									else {
										if(UE_rrc_inst[Mod_id].measReportList[i][j] != NULL)
											free(UE_rrc_inst[Mod_id].measReportList[i][j]);
											UE_rrc_inst[Mod_id].measReportList[i][j] = NULL;
									}
									break;
								case ReportConfigEUTRA__triggerType__event__eventId_PR_eventA4:
									break;
								case ReportConfigEUTRA__triggerType__event__eventId_PR_eventA5:
									break;
								default:
									LOG_D(RRC,"Invalid ReportConfigEUTRA__triggerType__event__eventId: %d", \
											UE_rrc_inst[Mod_id].ReportConfig[i][j]->reportConfig.choice.reportConfigEUTRA.triggerType.present);
									break;
							}
						}
					}
				}
			}
		}
	}
}

//Below routine implements Measurement Reporting procedure from 36.331 Section 5.5.5



/*
 *
first = 0;
  last = n - 1;
  middle = (first+last)/2;

  while( first <= last )
  {
     if ( array[middle] < search )
        first = middle + 1;
     else if ( array[middle] == search )
     {
        printf("%d found at location %d.\n", search, middle+1);
        break;
     }
     else
        last = middle - 1;

     middle = (first + last)/2;
  }
  if ( first > last )
     printf("Not found! %d is not present in the list.\n", search);
  */

#ifndef USER_MODE
EXPORT_SYMBOL(Rlc_info_am_config);
#endif
