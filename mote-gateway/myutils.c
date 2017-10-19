



#include <inttypes.h>
#include <stdio.h>

#include <errno.h>
#include "byteorder.h"
#include "thread.h"
#include "string.h"

#include "myutils.h"

/*
 * Dumps a packet in binary form
 */
void dump_hex(void* pData, size_t pLen)
{
    volatile uint8_t idx_char,idx_ligne,offset;
    #define PER_ROW 16
    char sss[PER_ROW+1];

        char c;
        // Ligne complete
        for(idx_ligne=0, offset=0; idx_ligne <= (pLen / PER_ROW ); idx_ligne++) {
            memset(sss, 0, PER_ROW+1);
            printf("~ ");

            // Chaque octet:
            for(idx_char=0; idx_char < PER_ROW; idx_char++, offset++) {
                // On dépasse la longueur du paket...
                if(offset >= pLen ) {
                    int z;for (z=idx_char+1; z < PER_ROW; z++) { printf("   ");} // pour aligner la derniere ligne
                    printf("   %s\r\n",sss);
                    return;
                }

                c=((char*)(pData))[idx_ligne*PER_ROW + idx_char] ;
                if (c >= ' ' && c <= '~') {
                    sss[idx_char] =c;
                } else sss[idx_char]='.';
                // Hex
                printf("%02x ",c);
            } // Fin ligne
            printf("%s\r\n",sss);
        }
        printf("\r\n");
    return;
}

