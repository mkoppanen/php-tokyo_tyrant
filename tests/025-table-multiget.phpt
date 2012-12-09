--TEST--
Test multiget on a table
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
?>
--XFAIL--
This test uses unsupported functionality
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$tt->vanish();

$rec = $tt->put('hi1', array('test1' => 'data1'));
$rec = $tt->put('hi2', array('test2' => 'data2'));
var_dump($tt->get(array('hi1', 'hi2')));


$brec = $tt->put("binary\0key", array("binary\0akey" => "b\0data"));

/* This output is wrong because the data seems to be separated by \0 */
var_dump($tt->get(array("binary\0key")));

var_dump($tt->get("binary\0key"));
	
?>
--EXPECTF--
array(2) {
  ["hi1"]=>
  array(1) {
    ["test1"]=>
    string(5) "data1"
  }
  ["hi2"]=>
  array(1) {
    ["test2"]=>
    string(5) "data2"
  }
}
array(1) {
  ["binarykey"]=>
  array(2) {
    ["binary"]=>
    string(4) "akey"
    ["b"]=>
    string(4) "data"
  }
}
array(1) {
  ["binaryakey"]=>
  string(6) "bdata"
}
