/* arith_decompress.cpp

   B. Bird - 2023-07-08
*/

#include <iostream>
#include <array>
#include <string>
#include <cassert>
#include <cstdint>
#include "input_stream.hpp"

const u32 EOF_SYMBOL = 256;


int main(int argc, char** argv){

    InputBitStream stream{std::cin};
    
    //Create a static frequency table with a frequency of 1 for 
    //all symbols except lowercase/uppercase letters (symbols 65-122)

    std::array<u32, EOF_SYMBOL+1> frequencies {};
    frequencies.fill(1);

    //Set the frequencies of letters (65 - 122) to 2 
    for(unsigned int i = 65; i <= 122; i++)
        frequencies.at(i) = 2;
    
    //Now set the frequencies of uppercase/lowercase vowels to 4 
    std::string vowels{"AEIOUaeiou"};
    for(unsigned char c: vowels)
        frequencies.at(c) = 4;


    //Now compute cumulative frequencies for each symbol.
    //We actually want the range [CF_low,CF_high] for each symbol,
    //but since CF_low(i) = CF_high(i-1), we only really have to compute
    //the array of lower bounds.

    //The cumulative frequency range for each symbol i will be 
    //[ CF_low.at(i), CF_low.at(i+1) ) 
    //(note that it's a half-open interval)
    std::array<u64, EOF_SYMBOL+2> CF_low {};
    CF_low.at(0) = 0;
    for (unsigned int i = 1; i < EOF_SYMBOL+2; i++){
        CF_low.at(i) = CF_low.at(i-1) + frequencies.at(i-1);
    }

    //We also need to know the global cumulative frequency (of all 
    //symbols), which will be the denominator of a formula below.
    //It turns out this value is already stored as CF_low.at(max_symbol+1)
    u64 global_cumulative_frequency = CF_low.at(EOF_SYMBOL+1);
    
    assert(global_cumulative_frequency <= 0xffffffff); //If this fails, frequencies must be scaled down


    u32 lower_bound = 0;
    u32 upper_bound = ~0;

    u32 encoded_bits = 0;
    for(int i = 0; i < 32; i++){
        encoded_bits = (encoded_bits<<1) | stream.read_bit();
    }


    while(1){
        //For safety, we will use u64 for all of our intermediate calculations.
        u64 current_range = (u64)upper_bound - (u64)lower_bound + 1;

        //Figure out which symbol comes next (we can definitely do better than linear 
        //search for this...)

        //First scale the encoded bitstring (which lies between lower_bound and upper_bound)
        //to the range [0, global_cumulative_frequency)
        //With pure real arithmetic, this is equivalent to the equation
        //  scaled = (encoded-low)*(global_cumulative_frequency/current_range),
        //however, we have to salt it with +1 and -1 terms (and rearrange it) to accommodate
        //fixed-point arithmetic.
        u64 scaled_symbol = (((u64)encoded_bits - lower_bound + 1)*global_cumulative_frequency - 1)/current_range;

        u32 symbol = 0;
        while(CF_low.at(symbol+1) <= scaled_symbol)
            symbol++;

        //If the symbol is the EOF marker, we're done
        if (symbol == EOF_SYMBOL)
            break;
            
        //Output the symbol
        std::cout << (char)symbol;

        //Now that we know what symbol comes next, we repeat the same process as the compressor
        //to prepare for the next iteration.

        u64 symbol_range_low = CF_low.at(symbol);
        u64 symbol_range_high = CF_low.at(symbol+1);
        upper_bound = lower_bound + (current_range*symbol_range_high)/global_cumulative_frequency - 1;
        lower_bound = lower_bound + (current_range*symbol_range_low)/global_cumulative_frequency;

        // <-- This is probably where we would adjust the frequency table if we used an adaptive model.

        //Even though we don't have to output bits, we do have to 
        //adjust the lower and upper bounds just like the compressor does.
        while(1){
            //Check if most significant bits (bit index 31) match.
            if ((upper_bound>>31) == (lower_bound>>31)){ 

                //Shift out the MSB of the lower bound, the upper bound and the encoded string
                //(Note that if lower and upper bounds have the same MSB, so does the encoded
                // bitstring)


                //Shift out the MSB of upper_bound (and shift in a 1 from the right)
                upper_bound <<= 1;
                upper_bound |= 1;
                
                //Shift out the MSB of lower_bound (and allow a 0 to be shifted in from the right)
                lower_bound <<= 1;
                
                //Shift out the MSB of encoded_bits (and bring in a new encoded bit from the
                //output file on the right)
                encoded_bits <<= 1;
                encoded_bits |= stream.read_bit();


            }else if ( ((lower_bound>>30)&0x1) == 1 && ((upper_bound>>30)&0x1) == 0){
                //If the MSBs didn't match, then the MSB of upper_bound must be 1 and
                //the MSB of lower_bound must be 0.
                //If we discover that lower_bound = 01... and upper_bound = 10... 
                //(which is what the if-statement above tests), then we have
                //to account for underflow.

                //If upper_bound = 10(xyz...), set upper_bound = 1(xyz...)
                //(that is, splice out the second-most-significant bit)
                upper_bound <<= 1;
                upper_bound |= (1U<<31);
                upper_bound |= 1;

                //If lower_bound = 01(abc...), set lower_bound = 0(abd...)
                lower_bound <<= 1;
                lower_bound &= (1U<<31) - 1; //i.e. 0x7fffffff

                //Since upper = 10... and lower = 01..., we know that 
                //either encoded_bits = 10... or encoded_bits = 01...
                //(since encoded_bits must be between lower and upper)
                //We want to splice out the second-most-significant bit
                //of encoded_bits (and bring in a new bit on the right)

                //Long way:
                {
                    u32 msb = encoded_bits>>31;
                    u32 rest = encoded_bits&0x3fffffff; //Bits 0 - 30
                    encoded_bits = (msb<<31)|(rest<<1)|stream.read_bit();
                }
                //Short way (tricky):
                //encoded_bits <<= 1; //Shift everything left (eliminating MSB)
                //encoded_bits = encoded_bits^(1<<31); //Flip bit 31 (which was previously bit 30, which we know was the opposite of the old bit 31)
                //encoded_bits |= stream.read_bit();
            }else{
                break;
            }
        }
    }
    
    return 0;
}