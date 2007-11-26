//=======================================================================
//			Copyright XashXT Group 2007 ©
//		roq_vq.h - adaptive GLA variant 3: heiarchial VQ
//=======================================================================

static void GLA_ITERATE(GLA_UNIT *input, uint inputCount, GLA_UNIT *book, uint bookSize, uint *mapping, uint *hits, uint *diameters, uint *diamvectors)
{
	uint	i, j;
	uint	misses;
	uint	diff;
	uint	bestDiff;
	uint	bestMatch;
	int	lhits[256];
	double	totalDeviation=0.0;

	// remap the diameter list
	for(i = 0; i < bookSize; i++) diameters[i] = diamvectors[i] = hits[i] = 0;

	// first map each input to the closest codebook entry
	for(i = 0; i < inputCount; i++)
	{
		bestDiff = GLA_DIFFERENCE(input + i, book);
		bestMatch = 0;
		for(j = 1; j < bookSize; j++)
		{
			diff = GLA_DIFFERENCE(input + i, book + j);
			if(diff < bestDiff)
			{
				bestDiff = diff;
				bestMatch = j;
				if(!diff) break;
			}
		}

		totalDeviation += (double)bestDiff / (double)inputCount;
		// mark this the most deviant if needed
		if(bestDiff > diameters[bestMatch])
		{
			diameters[bestMatch] = bestDiff;
			diamvectors[bestMatch] = i;
		}
		hits[bestMatch]++;
		// assign this entry to the centroid
		mapping[i] = bestMatch;
	}

	misses = 0;
	for(i = 0; i < bookSize; i++) lhits[i] = 0;
	for(i = 0; i < inputCount; i++) lhits[mapping[i]]++;
	for(i = 0; i < bookSize; i++) if(!lhits[i]) misses++;

	GLA_PRINTF("%i misses ", misses);
	GLA_CENTROID(input, inputCount, book, bookSize, mapping);
}

GLA_FUNCTION_SCOPE int GLA_FUNCTION(GLA_UNIT *input, uint inputCount, uint goalCells, uint *resultCount, GLA_UNIT **resultElements)
{
	GLA_UNIT	*output=GLA_NULL;
	GLA_UNIT	*output2=GLA_NULL;
	uint	*diameters;
	uint	*diamVectors;
	uint	*hits;
	GLA_UNIT	*tempOutput;
	uint	*mapping = GLA_NULL;
	uint	i,j,splitTotal;
	uint	bestDiff;
	uint	bestMatch;
	uint	step;
	uint	cellGoal;

	mapping = RQalloc(inputCount * sizeof(unsigned int));
	output = RQalloc(goalCells * sizeof(GLA_UNIT));
	output2 = RQalloc(goalCells * sizeof(GLA_UNIT));
	diameters = RQalloc(goalCells * sizeof(unsigned int));
	diamVectors = RQalloc(goalCells * sizeof(unsigned int));
	hits = RQalloc(goalCells * sizeof(unsigned int));

	if(!mapping || !output || !output2 || !diameters || !diamVectors || !hits)
	{
		if(mapping) Mem_Free(mapping);
		if(output) Mem_Free(output);
		if(output2) Mem_Free(output2);
		if(diameters) Mem_Free(diameters);
		if(diamVectors) Mem_Free(diamVectors);
		if(hits) Mem_Free(hits);
		return 0;
	}

	step = 0;

	// Map all input units to the same cell to create
	// a centroid for the entire input set
	for(i = 0; i < inputCount; i++) mapping[i] = 0;
	GLA_CENTROID(input, inputCount, output, 1, mapping);

	for(cellGoal = 1; cellGoal < goalCells; cellGoal *= 2)
	{
		GLA_PRINTF("stdvq: Building codebook size %i...", cellGoal*2);
		// split each cell into 2 new cells, then perturb them.
		// copy the original in so it can be perturbed as well if needed
		for(i = 0; i < cellGoal; i++)
		{
			GLA_COPY(output + i, output2 + (i*2));
			GLA_PERTURB(output2 + (i*2), output2 + (i*2+1), step);
		}

		// swap output buffers
		tempOutput = output2;
		output2 = output;
		output = tempOutput;

		// run GLA passes on the new output
		GLA_PRINTF("Refining... ", 0);
		for(i = 0; i < GLA2_MAX_PASSES; i++)
			GLA_ITERATE(input, inputCount, output, cellGoal*2, mapping, hits, diameters, diamVectors);
		GLA_PRINTF("Done\n", 0);
		step++;
	}

	// split high-diameter cells to minimize waste
	splitTotal = 0;
	for(i = 0; i < cellGoal; i++)
	{
		if(hits[i] == 0)
		{
			// This cell is dead.  Replace it with the highest-diameter input
			bestDiff = diameters[0];
			bestMatch = 0;

			for(j = 0; j < cellGoal; j++)
			{
				if(diameters[j] > bestDiff && diameters[j] != 0)
				{
					bestDiff = diameters[j];
					bestMatch = j;
				}
			}

			// if no split could be made, ignore...
			if(bestDiff == 0) break;
			splitTotal++;
			diameters[bestMatch] = 0;
			GLA_COPY(input + diamVectors[bestMatch], output + i);
		}
	}

	GLA_PRINTF("%i splits made.\n", splitTotal);
	Mem_Free(diameters);
	Mem_Free(diamVectors);
	Mem_Free(hits);
	Mem_Free(output2);
	Mem_Free(mapping);
	*resultCount = cellGoal;
	*resultElements = output;

	return 1;
}