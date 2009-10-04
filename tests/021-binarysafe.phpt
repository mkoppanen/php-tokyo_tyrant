--TEST--
Test string with null char in the middle
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

function output_ord($string, $len) {
	for ($i = 0; $i < $len; $i++) {
		echo ord($string[$i]) , " ";
	}
	echo "\n------------\n";
}

echo "Simple put\n";

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$var = 'START' . "\0" . "END"; 
output_ord($var, 9);

$tt->put("my key", $var);
$var = $tt->get("my key");
output_ord($var, 9);

$tt->vanish();

echo "Binary key\n";

$key = 'START' . "\0" . 'END';
$var = 'START' . "\0" . "END"; 

$tt->put($key, $var);

$var = $tt->get($key);
output_ord($var, 9);

echo "Binary key with prefix\n";

$key = 'START' . "\0" . 'END';
$var = 'START' . "\0" . "END"; 

ini_set("tokyo_tyrant.key_prefix", "my_prefix_");
$tt->put($key, $var);

$var = $tt->get($key);
output_ord($var, 9);

echo "Put multiple\n";

$var = 'START' . "\0" . "END"; 
$tt->put(array("k1" => $var, "k2" => $var));
$keys = $tt->get(array("k1", "k2"));

foreach ($keys as $value) {
	output_ord($value, 9);
}

echo "Put table\n";
$table = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$table->vanish();

$var = 'START' . "\0" . "END"; 
$rec = $table->put("hi", array("col1" => $var, "col2" => $var));
foreach ($keys as $value) {
	output_ord($value, 9);
}

?>
--EXPECT--
Simple put
83 84 65 82 84 0 69 78 68 
------------
83 84 65 82 84 0 69 78 68 
------------
Binary key
83 84 65 82 84 0 69 78 68 
------------
Binary key with prefix
83 84 65 82 84 0 69 78 68 
------------
Put multiple
83 84 65 82 84 0 69 78 68 
------------
83 84 65 82 84 0 69 78 68 
------------
Put table
83 84 65 82 84 0 69 78 68 
------------
83 84 65 82 84 0 69 78 68 
------------