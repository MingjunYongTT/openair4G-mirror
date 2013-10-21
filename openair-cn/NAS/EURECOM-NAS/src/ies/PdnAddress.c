#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#include "TLVEncoder.h"
#include "TLVDecoder.h"
#include "PdnAddress.h"

int decode_pdn_address(PdnAddress *pdnaddress, uint8_t iei, uint8_t *buffer, uint32_t len)
{
    int decoded = 0;
    uint8_t ielen = 0;
    int decode_result;
    if (iei > 0)
    {
        CHECK_IEI_DECODER(iei, *buffer);
        decoded++;
    }
    ielen = *(buffer + decoded);
    decoded++;
    CHECK_LENGTH_DECODER(len - decoded, ielen);
    pdnaddress->pdntypevalue = *(buffer + decoded) & 0x7;
    decoded++;
    if ((decode_result = decode_octet_string(&pdnaddress->pdnaddressinformation, ielen - 1, buffer + decoded, len - decoded)) < 0)
        return decode_result;
    else
        decoded += decode_result;
#if defined (NAS_DEBUG)
    dump_pdn_address_xml(pdnaddress, iei);
#endif
    return decoded;
}
int encode_pdn_address(PdnAddress *pdnaddress, uint8_t iei, uint8_t *buffer, uint32_t len)
{
    uint8_t *lenPtr;
    uint32_t encoded = 0;
    int encode_result;
    /* Checking IEI and pointer */
    CHECK_PDU_POINTER_AND_LENGTH_ENCODER(buffer, PDN_ADDRESS_MINIMUM_LENGTH, len);
#if defined (NAS_DEBUG)
    dump_pdn_address_xml(pdnaddress, iei);
#endif
    if (iei > 0)
    {
        *buffer = iei;
        encoded++;
    }
    lenPtr  = (buffer + encoded);
    encoded ++;
    *(buffer + encoded) = 0x00 |
    (pdnaddress->pdntypevalue & 0x7);
    encoded++;
    if ((encode_result = encode_octet_string(&pdnaddress->pdnaddressinformation, buffer + encoded, len - encoded)) < 0)
        return encode_result;
    else
        encoded += encode_result;
    *lenPtr = encoded - 1 - ((iei > 0) ? 1 : 0);
    return encoded;
}

void dump_pdn_address_xml(PdnAddress *pdnaddress, uint8_t iei)
{
    printf("<Pdn Address>\n");
    if (iei > 0)
        /* Don't display IEI if = 0 */
        printf("    <IEI>0x%X</IEI>\n", iei);
    printf("    <PDN type value>%u</PDN type value>\n", pdnaddress->pdntypevalue);
    dump_octet_string_xml(&pdnaddress->pdnaddressinformation);
    printf("</Pdn Address>\n");
}

