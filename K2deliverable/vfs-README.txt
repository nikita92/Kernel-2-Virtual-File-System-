Documentation for Kernel Assignment 2
=====================================

+-------------+
| BUILD & RUN |
+-------------+

Comments: 

#To clean up the project
make clean

# To build the project, First navigate to the appropriate folder, and type in 
make

#To execute the kernel 2 assignment, in Config.Mk change the VFS to 1 if not already to done
#Also the configurations for the previous assignment need to be in place. 
#Then type in 
./weenix -n    

+------+
| SKIP |
+------+

No test cases skipped.

+---------+
| GRADING |
+---------+

(A.1) In fs/vnode.c:
    (a) In special_file_read(): 6 out of 6 pts
    (b) In special_file_write(): 6 out of 6 pts

(A.2) In fs/namev.c:
    (a) In lookup(): 6 out of 6 pts
    (b) In dir_namev(): 10 out of 10 pts
    (c) In open_namev(): 2 out of 2 pts

(3) In fs/vfs_syscall.c:
    (a) In do_write(): 6 out of 6 pts
    (b) In do_mknod(): 2 out of 2 pts
    (c) In do_mkdir(): 2 out of 2 pts
    (d) In do_rmdir(): 2 out of 2 pts
    (e) In do_unlink(): 2 out of 2 pts
    (f) In do_stat(): 2 out of 2 pts

(B) vfstest: 39 out of 39 pts
    Comments: All 506 tests passed

(C.1) faber_fs_thread_test (3 out of 3 pts)
(C.2) faber_directory_test (2 out of 2 pts)

(D) Self-checks: (10 out of 10 pts)
    Comments: (please provide details, add subsections and/or items as needed)

Missing required section(s) in README file (vfs-README.txt): No
Submitted binary file : No
Submitted extra (unmodified) file : No
Wrong file location in submission : No
Use dbg_print(...) instead of dbg(DBG_PRINT, ...) : No
Not properly indentify which dbg() printout is for which item in the grading guidelines : No
Cannot compile : No
Compiler warnings : No
"make clean" : Yes it will happen
Useless KASSERT : None
Insufficient/Confusing dbg : None
Kernel panic : No
Cannot halt kernel cleanly : No

+------+
| BUGS |
+------+

Comments: None

+---------------------------+
| CONTRIBUTION FROM MEMBERS |
+---------------------------+

If not equal-share contribution, please list percentages.

+-------+
| OTHER |
+-------+

Special DBG setting in Config.mk for certain tests: No
Comments on deviation from spec (you will still lose points, but it's better to let the grader know): None
General comments on design decisions: No

