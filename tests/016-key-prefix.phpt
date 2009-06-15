--TEST--
Testing key prefix
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

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