--TEST--
Test removing records
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
include 'config.inc.php';
skip_if_table();
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put('key1', 'data');

var_dump($tt->get('key1'));
$tt->out('key1');
var_dump($tt->get('key1'));

$tt->vanish();
echo "---\n";

$tt->put(array('key1' => 'data1', 'key2' => 'data2'));
var_dump($tt->get(array('key1', 'key2')));
$tt->out(array('key1', 'key2'));
var_dump($tt->get(array('key1', 'key2')));
?>
--EXPECT--
string(4) "data"
NULL
---
array(2) {
  ["key1"]=>
  string(5) "data1"
  ["key2"]=>
  string(5) "data2"
}
array(0) {
}