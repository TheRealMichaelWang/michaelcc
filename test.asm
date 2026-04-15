
make_point:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -0	;reserve space for locals
	sw $a1, 0($a0);
	sw $a2, 0($a0);
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero
distance_squared:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -0	;reserve space for locals
	lw $v0, 0($a1);
	lw $a2, 0($a0);
	nand $a2, $a2, $a2
	addi $a2, $a2, 1
	add $a2, $a2, $v0
	lw $v0, 0($a1);
	lw $a0, 0($a0);
	nand $v0, $a0, $a0
	addi $v0, $v0, 1
	add $v0, $v0, $v0
	addi $sp, $sp, -4
	sw $a2, 3($sp)
	sw $a2, 2($sp)
	addi $at, $zero, 1
	sw $at, 1($sp)
	sw $zero, 0($sp)
sym1:
	lw $at, 1($sp)
	beq $at, $zero, sym3
	lw $a0, 3($sp)
	nand $at, $a0, $at
	nand $at, $at, $at
	beq $at, $zero, sym2
	lw $a0, 0($sp)
	lw $at, 2($sp)
	add $a0, $a0, $at
	sw $a0, 0($sp)
sym2:
	lw $at, 2($sp)
	add $at, $at, $at
	sw $at, 2($sp)
	lw $at, 1($sp)
	add $at, $at, $at
	sw $at, 1($sp)
	beq $zero, $zero, sym1
sym3:
	lw $a0, 0($sp)
	addi $sp, $sp, 4
	addi $sp, $sp, -4
	sw $v0, 3($sp)
	sw $v0, 2($sp)
	addi $at, $zero, 1
	sw $at, 1($sp)
	sw $zero, 0($sp)
sym4:
	lw $at, 1($sp)
	beq $at, $zero, sym6
	lw $v0, 3($sp)
	nand $at, $v0, $at
	nand $at, $at, $at
	beq $at, $zero, sym5
	lw $v0, 0($sp)
	lw $at, 2($sp)
	add $v0, $v0, $at
	sw $v0, 0($sp)
sym5:
	lw $at, 2($sp)
	add $at, $at, $at
	sw $at, 2($sp)
	lw $at, 1($sp)
	add $at, $at, $at
	sw $at, 1($sp)
	beq $zero, $zero, sym4
sym6:
	lw $v0, 0($sp)
	addi $sp, $sp, 4
	add $v0, $a0, $v0
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero
rect_width:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -0	;reserve space for locals
	lw $v0, 0($a0);
	lw $a0, 0($a0);
	nand $v0, $a0, $a0
	addi $v0, $v0, 1
	add $v0, $v0, $v0
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero
rect_height:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -0	;reserve space for locals
	lw $v0, 0($a0);
	lw $a0, 0($a0);
	nand $v0, $a0, $a0
	addi $v0, $v0, 1
	add $v0, $v0, $v0
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero
rect_area:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -0	;reserve space for locals
	add $a0, $a0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, rect_width
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	add $a0, $a0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, rect_height
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	addi $sp, $sp, -4
	sw $s0, 3($sp)
	sw $v0, 2($sp)
	addi $at, $zero, 1
	sw $at, 1($sp)
	sw $zero, 0($sp)
sym7:
	lw $at, 1($sp)
	beq $at, $zero, sym9
	lw $v0, 3($sp)
	nand $at, $v0, $at
	nand $at, $at, $at
	beq $at, $zero, sym8
	lw $v0, 0($sp)
	lw $at, 2($sp)
	add $v0, $v0, $at
	sw $v0, 0($sp)
sym8:
	lw $at, 2($sp)
	add $at, $at, $at
	sw $at, 2($sp)
	lw $at, 1($sp)
	add $at, $at, $at
	sw $at, 1($sp)
	beq $zero, $zero, sym7
sym9:
	lw $v0, 0($sp)
	addi $sp, $sp, 4
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero
move_point:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -0	;reserve space for locals
	lw $v0, 0($a0);
	add $v0, $v0, $a1
	sw $v0, 0($a0);
	lw $v0, 0($a0);
	add $v0, $v0, $a2
	sw $v0, 0($a0);
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero
main:
	addi $sp, $sp, -1	;save old frame pointer
	sw $fp, -1($sp)
	addi $fp, $sp, 0	;set frame pointer to current stack pointer
	sw $s0, -1($sp)	;saved callee saved register $s0
	sw $s1, -2($sp)	;saved callee saved register $s1
	sw $s2, -3($sp)	;saved callee saved register $s2
	add $sp, $sp, -3
	add $sp, $sp, -40	;reserve space for locals
	addi $s0, $fp, -8
	add $v0, $zero, 0
	sw $v0, -8($fp);
	add $v0, $zero, 0
	sw $v0, -4($fp);
	add $v0, $zero, 0
	sw $v0, -8($fp);
	add $v0, $zero, 0
	sw $v0, -8($fp);
	addi $s1, $fp, -16
	add $v0, $zero, 0
	sw $v0, -16($fp);
	add $v0, $zero, 0
	sw $v0, -12($fp);
	add $a0, $s1, $zero
	add $v0, $zero, 3
	add $a1, $v0, $zero
	add $v0, $zero, 4
	add $a2, $v0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, make_point
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	addi $a0, $fp, -24
	add $v0, $zero, 0
	sw $v0, -24($fp);
	add $v0, $zero, 0
	sw $v0, -20($fp);
	add $a0, $a0, $zero
	add $v0, $zero, 6
	add $a1, $v0, $zero
	add $v0, $zero, 8
	add $a2, $v0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, make_point
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	add $a0, $s0, $zero
	add $a1, $s1, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, distance_squared
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	addi $s0, $fp, -40
	add $v0, $zero, 0
	sw $v0, -40($fp);
	add $v0, $zero, 0
	sw $v0, -36($fp);
	add $v0, $zero, 0
	sw $v0, -32($fp);
	add $v0, $zero, 0
	sw $v0, -28($fp);
	lw $v0, -8($fp);
	sw $v0, -40($fp);
	lw $v0, -7($fp);
	sw $v0, -39($fp);
	lw $v0, -24($fp);
	sw $v0, -40($fp);
	lw $v0, -23($fp);
	sw $v0, -39($fp);
	add $a0, $s0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, rect_area
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	add $a0, $s0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, rect_width
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	add $a0, $s0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, rect_height
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	add $a0, $s1, $zero
	add $v0, $zero, 1
	add $a1, $v0, $zero
	add $v0, $zero, 1
	add $a2, $v0, $zero
	addi $sp, $sp, -1	;save return address to stack
	sw $ra, ($sp)
	la $at, move_point
	jalr $at, $ra
	lw $ra, 0($sp)	;restore return address from stack
	addi $sp, $sp, 1
	addi $sp, $sp, 0	;tear down parameters
	addi $sp, $sp, 0
	add $v0, $zero, 0
	add $sp, $fp, $zero
	lw $fp, 0($sp)
	add $sp, $sp, 1
	jalr $ra, $zero