#ifndef __VDM_BITFIELD_TRANSLATORS_H__
#define __VDM_BITFIELD_TRANSLATORS_H__

/*
 * Functions that convert bits into internal header representations...
 */
	UnstructuredVdmHeader 	getUnstructuredVdmHeader(uint32_t in);	// converts 32 bits into an unstructured vdm header struct
	StructuredVdmHeader 	getStructuredVdmHeader(uint32_t in);		// converts 32 bits into a structured vdm header struct
	IdHeader 				getIdHeader(uint32_t in);					// converts 32 bits into an ID Header struct
	VdmType 				getVdmTypeOf(uint32_t in);				// returns structured/unstructured vdm type
	
/*
 * Functions that convert internal header representations into bits...
 */
	uint32_t 	getBitsForUnstructuredVdmHeader(UnstructuredVdmHeader in);	// converts unstructured vdm header struct into 32 bits
	uint32_t 	getBitsForStructuredVdmHeader(StructuredVdmHeader in);		// converts structured vdm header struct into 32 bits 
	uint32_t 	getBitsForIdHeader(IdHeader in);							// converts ID Header struct into 32 bits 

/*
 * Functions that convert bits into internal VDO representations...
 */
	CertStatVdo 			getCertStatVdo(uint32_t in);
	ProductVdo 				getProductVdo(uint32_t in);
	CableVdo 				getCableVdo(uint32_t in);
	AmaVdo 					getAmaVdo(uint32_t in);

/*
 * Functions that convert internal VDO representations into bits...
 */	
 	uint32_t 	getBitsForProductVdo(ProductVdo in);	// converts Product VDO struct into 32 bits
	uint32_t 	getBitsForCertStatVdo(CertStatVdo in);	// converts Cert Stat VDO struct into 32 bits
	uint32_t	getBitsForCableVdo(CableVdo in);		// converts Cable VDO struct into 32 bits
	uint32_t	getBitsForAmaVdo(AmaVdo in);			// converts AMA VDO struct into 32 bits
	
#endif // header guard
	
