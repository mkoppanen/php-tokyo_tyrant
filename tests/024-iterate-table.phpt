--TEST--
Iterate a table storage
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$tt->vanish();

$tt->put("string key", array('test' => 'data', 'something' => "value is here"));
$tt->put(null, array('test' => 'changed data', 'something' => '1234'));

$iterator = new TokyoTyrantIterator($tt);

foreach ($iterator as $k => $r) {
	var_dump($k, $r);
}

$iterator = $tt->getIterator();

foreach ($iterator as $k => $r) {
	var_dump($k, $r);
}
	
?>
--EXPECTF--
string(10) "string key"
array(2) {
  ["test"]=>
  string(4) "data"
  ["something"]=>
  string(13) "value is here"
}
string(1) "1"
array(2) {
  ["test"]=>
  string(12) "changed data"
  ["something"]=>
  string(4) "1234"
}
string(10) "string key"
array(2) {
  ["test"]=>
  string(4) "data"
  ["something"]=>
  string(13) "value is here"
}
string(1) "1"
array(2) {
  ["test"]=>
  string(12) "changed data"
  ["something"]=>
  string(4) "1234"
}