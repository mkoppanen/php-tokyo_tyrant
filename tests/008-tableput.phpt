--TEST--
Table put variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
include 'config.inc.php';
skip_if_not_table();
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrantTable(TT_HOST, TT_PORT);
$tt->vanish();

$rec = $tt->put(null, array('test' => 'data', 'something' => time()));
var_dump($tt->get($rec));

$rec = $tt->put($rec, array('test' => 'changed data', 'something' => '1234'));
var_dump($tt->get($rec));

$rec = $tt->putcat($rec, array('col' => 'new item'));
var_dump($tt->get($rec));

try {
	$tt->putkeep($rec, array());
	echo "no exception\n";
} catch (TokyoTyrantException $e) {
	echo "got exception\n";
}	
	
?>
--EXPECTF--
array(2) {
  ["test"]=>
  string(4) "data"
  ["something"]=>
  string(10) "1244412251"
}
array(2) {
  ["test"]=>
  string(12) "changed data"
  ["something"]=>
  string(4) "1234"
}
array(3) {
  ["test"]=>
  string(12) "changed data"
  ["something"]=>
  string(4) "1234"
  ["col"]=>
  string(8) "new item"
}
got exception