@Assembly function for determing if the player is at the right border

.global is_player_right_border

is_player_right_border:
	@left shift x position by eight
	mov r0, r0, lsr #8
 
	@subtract sixteen and border from screen width
	sub r2, r2, #16
	sub r2, r2, r1

	@check if x position is less than screen width
	cmp r0, r2
	ble .less

	@return 1
	mov r0, #1
	b .rest

	.less:
	@return 0
		mov r0, #0

	.rest:
		mov pc, lr
