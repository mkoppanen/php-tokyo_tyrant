--TEST--
Test
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$colums = array("12345" => "test");
$tt->put("test", $colums);

var_dump($tt->get("test"));

?>
--EXPECT--
array(1) {
  [12345]=>
  string(4) "test"
}