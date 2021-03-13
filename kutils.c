/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: kutils.c                                                   *
* Description: Kernel space utilities and functions                *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      08-02-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#define INT2HEX8( s, num)          format_hex( s, num, 2,  0, 1, '0')
#define INT2HEX16( s, num)         format_hex( s, num, 4,  0, 1, '0')
#define INT2HEX32( s, num)         format_hex( s, num, 8,  0, 1, '0')
#define INT2HEX64( s, num)         format_hex( s, num, 16, 0, 1, '0')
#define INT2HEX8X( s, num)         format_hex( s, num, 2,  1, 1, '0')
#define INT2HEX16X( s, num)        format_hex( s, num, 4,  1, 1, '0')
#define INT2HEX32X( s, num)        format_hex( s, num, 8,  1, 1, '0')
#define INT2HEX64X( s, num)        format_hex( s, num, 16, 1, 1, '0')


void reverse(char *s);
void format_hex( char *str, uint64_t num, int spaces, int hexnotation, int lower, char c);
unsigned int krand( unsigned long seed, unsigned int max);
void itob(unsigned int n, char *s, int bits);
int str2hex( char *s, uint64_t *v);

/*
*******************************************************************************************************
* Utility functions source code                                                                       *
*******************************************************************************************************
*/

/* Reverse the string s */
void reverse(char *s){           /* reverse string s */
    int i, j, temp;
    for (i=0, j=strlen(s)-1; i<j; i++, j--) {
        temp = s[j];
        s[j] = s[i];
        s[i] = temp;
    } 
}


/* SGI stupid format specifier for printf() at kernel API doesn't works as it should. It 
 * does not format hex digits with adequate length, and so on.  
 * This function converts an unsigned number to a NULL ended C string. 
 * Arguments:
 *     str:         The hex string converted will be stored int this argument. It should have 
 *                  enough space preallocated for this. 
 *     num:         Number to convert. 
 *     spaces:      Width of the numeric part of converted number
 *     hexnotation: If its value = 1, it will prefix the string with '0x'
 *     lower:       If its value = 1, use low case letters for hex digits (a-f). Upper case else. 
 *     c:           The zero values at left will be filled with this char. 
 */
void format_hex( char *str, uint64_t num, int spaces, int hexnotation, int lower, char c){
    char s[128], *p, *a, *hex_begin;
	char lowerhex[] = "0123456789abcdef";
	char upperhex[] = "0123456789ABCDEF";
	uint64_t masked;
	int num_spac = 0;
	
	/* fill all will nulls */
	memset( (void *) s, 0, 128);
	
	
	p = s;
	if( lower) 
	    a = lowerhex; 
	else 
	    a = upperhex;
	
	if( hexnotation == 1){
	    strcpy( s, "0x");
	    p = p + 2;
	}

	hex_begin = p;
	
	/* fill the exact number of spaces with char c */
	memset( (void *) p, c, spaces );
	
	while( num > 0){
	    masked = num & 0x0f;
		*p++ = a[masked];
		num = num >> 4;
		num_spac++;
	}
	
    hex_begin[spaces] = '\0';
	
	reverse( hex_begin);
	strcpy( str, s);
}

/*
 * Generates a pseudo random number. 
 */ 
unsigned int krand( unsigned long seed, unsigned int max){
    unsigned int a, b, c; 
    unsigned int ac, seed0, seed1, seed2;
    unsigned int rc; 

    a = 0x0005; 
    b = 0xdeec; 
    c = 0xe66d; 

    ac = 0x0b;
    
    seed0 =  seed - 0xc2af;
    seed1 =  seed;
    seed2 =  seed + 0xd4a0c5;

    seed0 = ( (( seed1 * c) >> 16) + (( seed2 * b) >> 16) + (seed0 * c) + (seed1 *b) + (seed2 * a));
    seed1 = ( (( seed2 * c) >> 16) + ( seed1 * c) + ( seed2 * b));
    seed2 = ( seed2 * c) + ac;

    seed1 += seed2 >> 16; 
    seed0 += seed1 >> 16;
    
    seed0 &= 0xffff;
    seed1 &= 0xffff;

    rc = (seed0 << 16) | seed1;

    if( max > 0)
       rc = rc % max;
       
    return( rc);
}



void itob(unsigned int n, char *s, int bits){ /* convert int to binary string */
    int i=0;
  
    s[i++] = 'b';
    do {
        s[i++] = n % 2 + '0';
    } while ((n /= 2) != 0);
	
	for( ; i<bits; i++)
	    s[i] = '0';
	
	s[i] = '\0';
    reverse(s);
}

int str2hex( char *s, uint64_t *v){
    char str[256];
    char *p;
    uint64_t i;
    uint64_t t = 0;
    *v = 0;
     
   
    if( s == NULL)
        return( -1);
	   
    if( strlen( s) == 0)
	    return( -1);
   
    
   
    strcpy( str, s);
	p = str;
	
	/* if hex string begins with prefix '0x' or '0X' */ 
	if( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' )){
	    p = &str[2];
	}
	
	if( strlen( p) > 16)
	    return( -1);
		
	reverse( p);
	
	
	for( i = 0; i < strlen( p); i++){
	    if( p[i] >= '0' && p[i] <= '9' ){
		    t = t | (uint64_t)((uint64_t)( (uint64_t)p[i] - 48) << (i * 4));
		}else if( ( p[i] >= 'a' ) && ( p[i] <= 'f' )){
		    t = t | (uint64_t)((uint64_t)( (uint64_t)p[i] - 87) << (i * 4));
		}else if( ( p[i] >= 'A' ) && ( p[i] <= 'F' )){
		    t = t | (uint64_t)((uint64_t)( (uint64_t)p[i] - 55) << (i * 4));
		}else{
		    return( -2);
		}
	}
	
	*v = t;
	
	return( 0);
}
