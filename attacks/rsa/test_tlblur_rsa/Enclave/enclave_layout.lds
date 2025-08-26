/* enclave_layout.lds —— 只插入并对齐两个专用 .text 子段 */
SECTIONS
{
  /* 每个函数各自独占一页 */
  . = ALIGN(4096);
  .text.square_page : { KEEP(*(.text.square_page)) }

  . = ALIGN(4096);
  .text.multiply_page : { KEEP(*(.text.multiply_page)) }
}

/* 告诉 ld：把上面的两个段“插入到 .text 之后” */
INSERT AFTER .text;

