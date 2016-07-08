RainbowtableTools
============
# Profile #
A rainbow table is a precomputed table for reversing cryptographic hash functions, usually for cracking password hashes. Tables are usually used in recovering a plaintext password up to a certain length consisting of a limited set of characters. It is a practical example of a space/time trade-off, using less computer processing time and more storage than a brute-force attack which calculates a hash on every attempt, but more processing time and less storage than a simple lookup table with one entry per hash. Use of a key derivation function that employs a salt makes this attack infeasible.

## RainBowtableGen ##

Precomputed rainbowtables distributed by OpenMPI.

Hash Functions:

NTLM,MD5,SHA1

Some charset:

alpha                  = [ABCDEFGHIJKLMNOPQRSTUVWXYZ]
alpha-numeric          = [ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789]
alpha-numeric-symbol14 = [ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_+=]
all                    = [ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_+=~`[]{}|\:;"'<>,.?/]

numeric                = [0123456789]
loweralpha             = [abcdefghijklmnopqrstuvwxyz]
loweralpha-numeric     = [abcdefghijklmnopqrstuvwxyz0123456789]

##  RainBowCrack ##

This tool is to attack NTLM,MD5,SHA1 by rainbowtables;


# Change log #

- 2017.07.09

最初始的版本，后续再完善...


# 社区讨论 #
- QQ Group :`欢迎加入开源Web爬虫QQ群:322937592`
- 个人主页 : http://www.cnblogs.com/liinux/