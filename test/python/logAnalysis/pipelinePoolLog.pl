#!/usr/bin/perl

while(<>)
{
	($timeStamp) = (/^([\d\.]+)/);

	if (/TX.*com.webos.media .*\"loadCompleted\"/)
	{
		#print;
		$loadCompleteTime = $timeStamp;
		$deltaTime = $loadCompleteTime - $loadTime;
		#print ("$loadTime $loadCompleteTime $deltaTime\n");

		$totalTime += $deltaTime;
		$iterations++;
	}
	elsif (/RX.*com.webos.media .*\"loadCompleted\"/)
	{
		#print;
	}
	elsif (/TX.*\/\/load/)
	{
		#print("$timeStamp TX load\n");
	}
	elsif (/RX.*\/\/load/)
	{
		#print;
		$loadTime = $timeStamp;
	}
}

$averageTime = $totalTime / $iterations;

print "Iterations: $iterations\n";
print "Total time: $totalTime\n";
print "Average time: $averageTime\n";