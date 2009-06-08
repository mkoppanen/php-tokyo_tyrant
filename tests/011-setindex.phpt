--TEST--
Test setting an index on column
--SKIPIF--
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$tt->vanish();

$tt->put(null, array('test' => 'data0', 'something' => time()));
$tt->put(null, array('test' => 'data1', 'something' => time()));
$tt->put(null, array('test' => 'data2', 'something' => time()));
$tt->put(null, array('test' => 'data3', 'something' => time()));
$tt->put(null, array('test' => 'data4', 'something' => time()));

try {
	$tt->setIndex('test', TokyoTyrant::RDBIT_LEXICAL);
	echo "index set\n";
} catch (TokyoTyrantException $e) {
	echo "failed to set index\n";
}
	
?>
--EXPECT--
index set