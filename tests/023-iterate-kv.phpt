--TEST--
Iterator test
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put('cherry',     'red');
$tt->put('strawberry', 'red');
$tt->put('apple',      'green');
$tt->put('lemon',      'yellow');

$iterator = new TokyoTyrantIterator($tt);

foreach ($iterator as $k => $v) {
	echo "$k => $v\n";
}

$iterator = $tt->getIterator();
foreach ($iterator as $k => $v) {
	echo "$k => $v\n";
}

?>
--EXPECT--
cherry => red
strawberry => red
apple => green
lemon => yellow
cherry => red
strawberry => red
apple => green
lemon => yellow