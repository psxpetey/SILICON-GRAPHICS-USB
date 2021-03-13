/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: dumphex.c                                                  *
* Description: Byte dump utility function                          *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      08-02-2012     -Fixes in appearance of hex digits. Now dumps looks way better           *
*                                                                                                     *
* BSDero      05-09-2012     -Much better dump appearance                                             *
*                                                                                                     *
* BSDero      01-16-2012     -Fixes for better dump appearance                                        *
*                                                                                                     *
* BSDero      01-05-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/


int dumphex( uint64_t class, uchar_t level, unsigned char *s, size_t len);
void dump_uint32( uint64_t class, uchar_t level, uint32_t *u32, size_t size);

void dump_uint32( uint64_t class, uchar_t level, uint32_t *u32, size_t size){
    int i, j;
    unsigned char *p;
    char s1[128], s2[128];
    
    if( global_trace_class.level < level) 
	return;
	
    if((class & global_trace_class.class) == 0)
	return;
   
    size /= 4;
    printf("--------  ------------  ----\n");
    for( i = 0; i < size; i++){
	    INT2HEX16X( s1, i);
	    INT2HEX32X( s2, u32[i]);
        printf("[%s]  [%s]  ", s1, s2); 
	    p = (unsigned char *) &u32[i];
        for( j = 0; j < 4; j++ ){
	        if( isprint( *p)){
	            printf("%c", *p);
	        }else{
                printf(".");		
            }
            p++;
	    }
	    
        printf("\n");
    }
}

int dumphex( uint64_t class, uchar_t level, unsigned char *p, size_t size){
    unsigned int i, l, m;
    char s[128];
	
    if( global_trace_class.level < level) 
	    return(0);
	
    if((class & global_trace_class.class) == 0)
	    return(0);
            	
    printf("        _0 _1 _2 _3 _4 _5 _6 _7  _8 _9 _A _B _C _D _E _F    01234567 89ABCDEF\n");
    printf("        -----------------------  -----------------------    -------- --------\n");
	
    m = l = 0;
    do{
		INT2HEX16X( s, l);
        printf("%s  ", s);
	    for( i = 0; i < 8; i++, l++){
	        if( l < size){
				INT2HEX8( s, p[i] );
	            printf("%s ", s);
	        }else{
		        printf("   ");
		    }
	    }
		
	    printf(" ");
	    for( ; i < 16; i++, l++){
	        if( l < size){
				INT2HEX8( s, p[i] );
	            printf("%s ", s);
	        }else{
		        printf("   ");
		    }
	    }
		
		printf("   ");
		for( i = 0; i < 8; i++, m++){
			if( m < size)
				if( isprint( p[i] ))
					printf("%c", p[i]);
				else
					printf(".");
			else
				printf(" ");
		}
		
		printf(" ");
		for( ; i < 16; i++, m++){
			if( m < size)
				if( isprint( p[i] ))
					printf("%c", p[i]);
				else
					printf(".");
			else
				printf(" ");
		}
		
		p = p + 16;
		printf("\n");
		
		
    }while( l < size);
    printf("\n");
    
    return(0);
}

