--TEST--
Test variations of clone
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt2 = clone $tt;

try {
	$tt2->stat();
	echo "Object is connected\n";
} catch (TokyoTyrantException $e) {
	echo "Object is not connected\n";
}

unset($tt);
unset($tt2);

$tt = new TokyoTyrant();
$tt2 = clone $tt;

try {
	$tt2->stat();
	echo "Object is connected\n";
} catch (TokyoTyrantException $e) {
	echo "Object is not connected\n";
}

unset($tt);
unset($tt2);

echo "done";
?>
--EXPECT--
Object is connected
Object is not connected
done