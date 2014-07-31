/**
 * Author: Luis Alejandro GonzÃ¡lez Miranda.
 *
 * Parameters:
 * number - The number you want to lookup
 * scale - The scale of the number, the total of elements in the curve. The number should be in the range [0 .. scale[
 * cx, cy - The returned coordinates in terms of qs (see below)
 */
void hilpoint(long number, long scale, double qs, double *cx, double *cy)
{
	/* This is a description of the shape of each fundamental pattern */
	int patterns[4][4]={{0,1,3,2},{0,2,3,1},{3,1,0,2},{3,2,0,1}};

	/* This is a description of what happens to each pattern the next iteration */
	int transforms[4][4]={{1,0,2,0},{0,3,1,1},{2,2,0,3},{3,1,3,2}};

	/* Quadrant coordinates in pixels; should be initialized to the width
	 * and height of your arena, which must be square.
	 * The coordinates will be expressed in numbers in the range [0..qs[
	 */

	/* This variable should be initialized according to the scale of the
	 * numbers at the Hilbert curve. We will need to divide by this number,
	 * and the integer part of the result should be always between 0 and 3.
	 *
	 * For example, if you're making a map of Class A IP addresses, your
	 * range is [0..256[; therefore, the value of divisor is 256/4, or 64;
	 */
	long divisor=scale/4;

	/* A copy of your number */
	long tmp=number;

	/* The start pattern. Don't modify */
	long pattern=1;

	(*cx)=(*cy)=0.0;

	while(divisor>0) {
		int cell;
		long mantissa;

		mantissa=tmp/divisor;
		tmp=tmp%divisor;
		divisor/=4;
		cell=patterns[pattern][mantissa];
		pattern=transforms[pattern][cell];
		qs/=2.0;

		if(cell & 1) (*cx)+=qs;
		if(cell & 2) (*cy)+=qs;
	}
}

