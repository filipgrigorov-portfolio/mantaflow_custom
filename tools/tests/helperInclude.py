#
# Helper functions for test runs in mantaflow
# 

from manta import *
import os
import shutil




def outputFilename( file, gridname ):
	return file +"_"+ gridname + "_out.uni" 

def referenceFilename( file, gridname ):
	return file +"_"+ gridname + "_ref.uni" 


def checkResult( name, result , thresh, threshStrict, invertResult=False ):
	curr_thresh = thresh
	if(getStrictSetting()==1):
		curr_thresh = threshStrict
	print ("Checking '%s', result=%f , thresh=%f" % ( name , result , curr_thresh) )

	if   ( ( result > 0.) and (result < 1e-04) ):
		print ("Debug, small difference: %f (output scaled by 1e5)" % ( result * 1e05 ) ) # debugging...
	elif ( ( result > 0.) and (result < 1e-08) ):
		print ("Debug, small difference: %f (output scaled by 1e9)" % ( result * 1e09 ) ) # debugging...
	#elif ( result == 0.0):
		#print ("Result is really zero...")

	allGood = 0
	if ( result <= curr_thresh) :
		allGood = 1

	# for checks that should fail
	if ( invertResult == True) :
		if ( allGood == 0) :
			allGood = 1
		else:
			allGood = 0

	# now react on outcome...
	if ( allGood == 1 ):
		print("OK! Results for "+name+" match...")
		return 0
	else:
		print("FAIL! Allowed "+name+" threshold "+str(curr_thresh)+", results differ by "+str(result))
		return 1


def getGenRefFileSetting( ):
	# check env var for generate data setting
	ret = int(os.getenv('MANTA_GEN_TEST_DATA', 0))
	# print("Gen-data-setting: " + str(ret))
	if(ret>0):
		return 1
	return 0

def getStrictSetting( ):
	# check env var whether strict mode enabled
	ret = int(os.getenv('MANTA_TEST_STRICT', 0))
	print("Strict-test-setting: " + str(ret))
	if(ret>0):
		return 1
	return 0


# compare a grid, in generation mode (MANTA_GEN_TEST_DATA=1) it
# creates the data on disk, otherwise it loads the disk data,
# computes the largest per cell error, and checks whether it matches
# the allowed thresholds
#
# note, there are two thresholds:
# 	- the "normal" one is intended for less strict comparisons of versions from different compilers
#	- the "strict" one (enbable with "export MANTA_TEST_STRICT=1") is for comparing different version 
#		generated with the same compiler
#
def doTestGrid( file , name, solver , grid, threshold=0, thresholdStrict=0, invertResult=False ):
	# both always have to given together (if not default)
	if ( threshold!=0 and thresholdStrict==0 ):
		print( "Error doTestGrid - give both thresholds at the same time...")
		return 1
	if ( threshold==0 and thresholdStrict!=0 ):
		print( "Error doTestGrid - give both thresholds at the same time...")
		return 1

	# handle grid types that need conversion
	#print( "doTestGrid, incoming grid type :" + type(grid).__name__)
	if ( type(grid).__name__ == "MACGrid" ):
		gridTmpMac = solver.create(VecGrid)
		convertMacToVec3(grid , gridTmpMac )
		return doTestGrid( file, name, solver, gridTmpMac , threshold, thresholdStrict)
	if ( type(grid).__name__ == "LevelsetGrid" ):
		gridTmpLs = solver.create(RealGrid)
		convertLevelsetToReal(grid , gridTmpLs )
		return doTestGrid( file, name, solver, gridTmpLs  , threshold, thresholdStrict)
	if ( type(grid).__name__ == "IntGrid" ):
		print( "Error doTestGrid - int grids not yet supported...")
		return 1

	# now we should only have real & vec3 grids

	# create temp grid
	if ( type(grid).__name__ == "RealGrid" ):
		compareTmpGrid = solver.create(RealGrid)
	elif ( type(grid).__name__ == "VecGrid" ):
		compareTmpGrid = solver.create(VecGrid)
	else:
		print( "Error doTestGrid - unknown grid type " + type(grid).__name__ )
		return 1

	genRefFiles = getGenRefFileSetting()

	if (genRefFiles==1):
		#grid.save( outputFilename( file, name ) )
		#shutil.copyfile( outputFilename( file, name ) , referenceFilename( file, name ) )
		grid.save( referenceFilename( file, name ) )
		print( "OK! Generated reference file '" + referenceFilename( file, name ) + "'")
		return 0
	else:
		compareTmpGrid.load( referenceFilename( file, name ) )

		errVal = 1e10
		if ( type(grid).__name__ == "RealGrid" ):
			errVal = gridMaxDiff    ( grid, compareTmpGrid )
		elif ( type(grid).__name__ == "VecGrid" ):
			errVal = gridMaxDiffVec3( grid, compareTmpGrid )

		# finally, compare max error to allowed threshold, and return result
		return checkResult( name, errVal , threshold , thresholdStrict, invertResult )




