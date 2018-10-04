#!/usr/bin/perl

my $i = 0;

print "static int\nhandle_invalid (u1 * bc, java_class_t * cls) {\n";
print "\tHB_ERR(\"%s NOT IMPLEMENTED\\n\", __func__);\n";
print "\treturn -1;\n";
print "}\n\n";

while (<STDIN>) {

	my @a = split(" ", $_);

	if ($i != $a[0]) {
		while ($i != $a[0]) {
			$i += 1;
		}
	} 

	print "static int\nhandle_$a[2] (u1 * bc, java_class_t * cls) {\n";
	print "\tHB_ERR(\"%s NOT IMPLEMENTED\\n\", __func__);\n";
	print "\treturn -1;\n";
	print "}\n\n";

	$i += 1;

}

