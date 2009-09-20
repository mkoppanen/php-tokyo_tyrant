--TEST--
Table search variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$tt->sync()->vanish();

$tt->put(null, array('test' => 'abc123111'));
$tt->put(null, array('test' => 'cde456111'));
$tt->put(null, array('test' => 'abba'));

$query = $tt->getQuery();
$query->addCond('test', TokyoTyrant::RDBQC_STREQ, 'cde456111');
var_dump($query->search());

echo "------------------\n";

var_dump($query->count());

echo "------------------\n";

$query = $tt->getQuery();
$query->addCond('test', TokyoTyrant::RDBQC_STREQ, 'abc123111');
var_dump($query->search());

echo "------------------\n";

$query = $tt->getQuery();
$query->addCond('test', TokyoTyrant::RDBQC_STREW, '111');
var_dump($query->search());

echo "------------------\n";

var_dump($query->count());

echo "------------------\n";

$query->setLimit(null, null);
var_dump($query->search());

echo "------------------\n";

$query->setLimit(1);
var_dump($query->search());

echo "------------------\n";

$query->setLimit(1, 1);
var_dump($query->search());

echo "------------------\n";

$query->setOrder('test', TokyoTyrant::RDBQO_STRDESC);
var_dump($query->search());

echo "------------------\n";

$query->setOrder('test', TokyoTyrant::RDBQO_STRASC);
var_dump($query->search());

echo "------------------\n";

var_dump($query->count());

echo "------------------\n";

$query = $tt->getQuery();
var_dump($query->search());

echo "------------------\n";

var_dump($query->count());

echo "------------------\n";

?>
--EXPECTF--
array(1) {
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------
int(1)
------------------
array(1) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
}
------------------
array(2) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------
int(2)
------------------
array(2) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------
array(1) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
}
------------------
array(1) {
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------
array(1) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
}
------------------
array(1) {
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------
int(1)
------------------
array(3) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
  [3]=>
  array(1) {
    ["test"]=>
    string(4) "abba"
  }
}
------------------
int(3)
------------------
