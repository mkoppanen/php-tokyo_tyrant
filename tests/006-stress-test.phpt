--TEST--
Test put, get and out in a batch
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$values = 10000;
$tt = new TokyoTyrant(TT_HOST, TT_PORT);

$time = microtime(true);
echo "Putting {$values} values..";
for ($i = 0; $i < $values; $i++) {
	$tt->put("key_{$i}", "data_{$i}");
}
echo " took: ";
$took = microtime(true) - $time;
echo $took , "\n";

$time = microtime(true);
echo "Getting {$values} values..";
for ($i = 0; $i < $values; $i++) {
	$tt->get("key_{$i}");
}
echo " took: ";
$took = microtime(true) - $time;
echo $took , "\n";

$time = microtime(true);

echo "Deleting {$values} values..";
for ($i = 0; $i < $values; $i++) {
	$tt->out("key_{$i}");
}
echo " took: ";
$took = microtime(true) - $time;
echo $took , "\n";

echo "-----------\n";

?>
--EXPECTF--
Putting %d values.. took: %f
Getting %d values.. took: %f
Deleting %d values.. took: %f
-----------