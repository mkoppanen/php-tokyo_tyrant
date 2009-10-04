--TEST--
Test string with null char in the middle of key
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$key = 'START' . "\0" . 'END';
$var = 'test data'; 

$tt->put($key, $var);

/* This should fail */
var_dump($tt->get("START"));
var_dump($tt->get($key));

/* test put shl */
$tt->put($key, "abc"); 
$tt->putshl($key, "de", 4); 
var_dump($tt->get($key));

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

ini_set("tokyo_tyrant.key_prefix", "my_prefix_");
$tt->put($key, $var);

var_dump($tt->get("START"));
var_dump($tt->get($key));

?>
--EXPECT--
NULL
string(9) "test data"
string(4) "bcde"
NULL
string(9) "test data"