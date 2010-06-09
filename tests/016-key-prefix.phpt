--TEST--
Testing key prefix
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

ini_set("tokyo_tyrant.key_prefix", "my_prefix_");
$tt->put("key", "value");
var_dump($tt->get("key"));

ini_set("tokyo_tyrant.key_prefix", "another_prefix_");
var_dump($tt->get("key"));
$tt->put("key", "value");
var_dump($tt->get("key"));

$tt->put(array("k" => "v", "k1" => "v1"));
var_dump($tt->get(array("k", "k1")));
$tt->out(array("k", "k1"));
var_dump($tt->get(array("k", "k1")));

ini_set("tokyo_tyrant.key_prefix", "my_prefix_");
var_dump($tt->get("key"));
$tt->out("key");
var_dump($tt->get("key"));

ini_set("tokyo_tyrant.key_prefix", null);
$tt->put("key", "value");
var_dump($tt->get("key"));
$tt->out("key");
var_dump($tt->get("key"));

ini_set("tokyo_tyrant.key_prefix", "my_prefix_");

$table = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$table->vanish();

$rec = $table->put(null, array('test' => 'data', 'something' => time()));
var_dump($rec);

$rec = $table->put(null, array('test' => 'data', 'something' => time()));
var_dump($rec);


?>
--EXPECT--
string(5) "value"
NULL
string(5) "value"
array(2) {
  ["k"]=>
  string(1) "v"
  ["k1"]=>
  string(2) "v1"
}
array(0) {
}
string(5) "value"
NULL
string(5) "value"
NULL
int(1)
int(2)