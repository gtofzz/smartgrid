#ifndef CONTROL_H
#define CONTROL_H
void control_start_threads(void);
void control_join_threads(void);

/* Para UI mostrar a prévia (2*Gap) */
int control_preview_shedding_ids(int epoch, int gap, int *out_ids, int max_out);
#endif
