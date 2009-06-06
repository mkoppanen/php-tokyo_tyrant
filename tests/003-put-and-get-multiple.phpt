--TEST--
Test multiple value put / get variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
skip_if_table();
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put(array('key1' => 'data1', 'key2' => 'data2'));
var_dump($tt->get(array('key1', 'key2')));

$tt->putcat(array('key1' => '---', 'key2' => '---'));
var_dump($tt->get(array('key1', 'key2')));

try {
	$tt->putkeep(array('key1' => '---', 'key2' => '---'));
	var_dump($tt->get(array('key1', 'key2')));
} catch (TokyoTyrantException $e) {
	echo "got exception {$e->getMessage()}\n";
}
$tt->vanish();

$tt->putkeep(array('k1' => "data", "k2" => 'data'));
echo $tt->get('k1') . "\n";
?>
--EXPECT--
array(2) {
  ["key1"]=>
  string(5) "data1"
  ["key2"]=>
  string(5) "data2"
}
array(2) {
  ["key1"]=>
  string(8) "data1---"
  ["key2"]=>
  string(8) "data2---"
}
got exception
data