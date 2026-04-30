#ifndef TASKS_H
#define TASKS_H

int task_add_user(const char *phonenr, const char *who);
int task_delete_user(const char *phonenr, const char *who);
int task_promote_user(const char *phonenr, const char *who);
int task_demote_user(const char *phonenr, const char *who);
int task_swap_admin(const char *phonenr, const char *who);
void task_info_line(char *line, uint8_t i);
void task_rshutter_up();
void task_rshutter_down();
void task_overhead_down();

#endif